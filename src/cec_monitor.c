/**
 * @file cec_monitor.c
 * @brief CEC Monitor Implementation (Platform Refactor Version)
 * 
 * 重構內容:
 * 1. 移除直接的 cec-ctl 調用
 * 2. ⭐ 使用 platform_get_ps5_power() 接口
 * 3. 簡化狀態查詢邏輯
 * 
 * @version 2.0.1
 * @date 2024-11-18
 */

#include "cec_monitor.h"

#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming-platform/platform_interface.h>
    #include <gaming/logger.h>
  #else
    #include "../../gaming-platform/src/platform_interface.h"
    #include "../../gaming-core/src/logger.h"
  #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* ============================================================
 *  Constants
 * ============================================================ */

#define CEC_POLL_INTERVAL_MS    5000    // 每5秒輪詢一次
#define CEC_MAX_CONSECUTIVE_ERRORS  5   // 最大連續錯誤次數

/* ============================================================
 *  Type Definitions
 * ============================================================ */

typedef struct {
    bool initialized;
    bool monitoring;
    
    // 監控執行緒
    pthread_t monitor_thread;
    pthread_mutex_t mutex;
    
    // PS5狀態
    ps5_power_state_t current_state;
    ps5_power_state_t last_state;
    time_t last_update_time;
    int consecutive_errors;
    
    // 回調函數
    ps5_state_callback_t state_callback;
    void *callback_data;
    
} cec_monitor_context_t;

/* ============================================================
 *  Global Variables
 * ============================================================ */

static cec_monitor_context_t g_cec_ctx = {0};

/* ============================================================
 *  Helper Functions
 * ============================================================ */

/**
 * @brief 查詢PS5電源狀態
 * @return PS5電源狀態
 */
static ps5_power_state_t query_power_status(void) {
#ifdef TESTING
    // 測試模式: 返回當前狀態
    return g_cec_ctx.current_state;
#else
    // ⭐ 使用 platform 接口查詢 PS5 電源狀態
    platform_ps5_power_t power = platform_get_ps5_power();
    
    switch (power) {
        case PLATFORM_PS5_ON:
            return PS5_POWER_ON;
        case PLATFORM_PS5_STANDBY:
            return PS5_POWER_STANDBY;
        case PLATFORM_PS5_OFF:
            return PS5_POWER_OFF;
        default:
            return PS5_POWER_UNKNOWN;
    }
#endif
}

/**
 * @brief 改變PS5狀態
 */
static void change_ps5_state(ps5_power_state_t new_state) {
    if (new_state == g_cec_ctx.current_state) {
        return;
    }
    
    pthread_mutex_lock(&g_cec_ctx.mutex);
    
    ps5_power_state_t old_state = g_cec_ctx.current_state;
    g_cec_ctx.current_state = new_state;
    g_cec_ctx.last_state = old_state;
    g_cec_ctx.last_update_time = time(NULL);
    
    pthread_mutex_unlock(&g_cec_ctx.mutex);
    
    #ifndef TESTING
    logger_info("PS5 state changed: %s -> %s",
               ps5_power_state_to_string(old_state),
               ps5_power_state_to_string(new_state));
    #endif
    
    // 觸發回調
    if (g_cec_ctx.state_callback) {
        g_cec_ctx.state_callback(new_state, g_cec_ctx.callback_data);
    }
}

/**
 * @brief 監控執行緒函數
 */
static void* monitor_thread_func(void *arg) {
    (void)arg;
    
    #ifndef TESTING
    logger_info("CEC monitor thread started");
    #endif
    
    while (g_cec_ctx.monitoring) {
        // 查詢PS5狀態
        ps5_power_state_t state = query_power_status();
        
        if (state != PS5_POWER_UNKNOWN) {
            // 查詢成功
            g_cec_ctx.consecutive_errors = 0;
            
            // 狀態改變時更新
            if (state != g_cec_ctx.current_state) {
                change_ps5_state(state);
            }
        } else {
            // 查詢失敗
            g_cec_ctx.consecutive_errors++;
            
            #ifndef TESTING
            logger_warning("Failed to query PS5 status (consecutive errors: %d)",
                       g_cec_ctx.consecutive_errors);
            #endif
            
            // 超過最大錯誤次數,將狀態設為未知
            if (g_cec_ctx.consecutive_errors >= CEC_MAX_CONSECUTIVE_ERRORS) {
                if (g_cec_ctx.current_state != PS5_POWER_UNKNOWN) {
                    change_ps5_state(PS5_POWER_UNKNOWN);
                }
            }
        }
        
        // 等待下次輪詢
        usleep(CEC_POLL_INTERVAL_MS * 1000);
    }
    
    #ifndef TESTING
    logger_info("CEC monitor thread stopped");
    #endif
    
    return NULL;
}

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

int cec_monitor_init(void) {
    if (g_cec_ctx.initialized) {
        #ifndef TESTING
        logger_warning("CEC monitor already initialized");
        #endif
        return 0;
    }
    
    // 初始化context
    memset(&g_cec_ctx, 0, sizeof(g_cec_ctx));
    pthread_mutex_init(&g_cec_ctx.mutex, NULL);
    
    g_cec_ctx.current_state = PS5_POWER_UNKNOWN;
    g_cec_ctx.last_state = PS5_POWER_UNKNOWN;
    g_cec_ctx.consecutive_errors = 0;
    g_cec_ctx.initialized = true;
    
    #ifndef TESTING
    logger_info("CEC monitor initialized (using platform interface)");
    #endif
    
    return 0;
}

void cec_monitor_cleanup(void) {
    if (!g_cec_ctx.initialized) {
        return;
    }
    
    // 停止監控
    if (g_cec_ctx.monitoring) {
        cec_monitor_stop();
    }
    
    pthread_mutex_destroy(&g_cec_ctx.mutex);
    memset(&g_cec_ctx, 0, sizeof(g_cec_ctx));
    g_cec_ctx.initialized = false;
    
    #ifndef TESTING
    logger_info("CEC monitor cleaned up");
    #endif
}

int cec_monitor_start(void) {
    if (!g_cec_ctx.initialized) {
        return -1;
    }
    
    if (g_cec_ctx.monitoring) {
        return 0;  // 已經在監控
    }
    
    g_cec_ctx.monitoring = true;
    
    // 啟動監控執行緒
    if (pthread_create(&g_cec_ctx.monitor_thread, NULL, 
                      monitor_thread_func, NULL) != 0) {
        #ifndef TESTING
        logger_error("Failed to create monitor thread");
        #endif
        g_cec_ctx.monitoring = false;
        return -1;
    }
    
    #ifndef TESTING
    logger_info("CEC monitoring started");
    #endif
    
    return 0;
}

void cec_monitor_stop(void) {
    if (!g_cec_ctx.initialized || !g_cec_ctx.monitoring) {
        return;
    }
    
    g_cec_ctx.monitoring = false;
    
    // 等待執行緒結束
    pthread_join(g_cec_ctx.monitor_thread, NULL);
    
    #ifndef TESTING
    logger_info("CEC monitoring stopped");
    #endif
}

ps5_power_state_t cec_monitor_get_state(void) {
    if (!g_cec_ctx.initialized) {
        return PS5_POWER_UNKNOWN;
    }
    
    pthread_mutex_lock(&g_cec_ctx.mutex);
    ps5_power_state_t state = g_cec_ctx.current_state;
    pthread_mutex_unlock(&g_cec_ctx.mutex);
    
    return state;
}

time_t cec_monitor_get_last_update_time(void) {
    if (!g_cec_ctx.initialized) {
        return 0;
    }
    
    pthread_mutex_lock(&g_cec_ctx.mutex);
    time_t update_time = g_cec_ctx.last_update_time;
    pthread_mutex_unlock(&g_cec_ctx.mutex);
    
    return update_time;
}

void cec_monitor_set_callback(ps5_state_callback_t callback, void *user_data) {
    g_cec_ctx.state_callback = callback;
    g_cec_ctx.callback_data = user_data;
}

const char* ps5_power_state_to_string(ps5_power_state_t state) {
    switch (state) {
        case PS5_POWER_OFF:     return "OFF";
        case PS5_POWER_STANDBY: return "STANDBY";
        case PS5_POWER_ON:      return "ON";
        case PS5_POWER_UNKNOWN: return "UNKNOWN";
        default:                return "INVALID";
    }
}

/* ============================================================
 *  測試輔助函數 (僅供測試使用)
 * ============================================================ */

#ifdef TESTING

/**
 * @brief 模擬PS5狀態改變 (測試用)
 */
void cec_monitor_test_set_state(ps5_power_state_t state) {
    change_ps5_state(state);
}

/**
 * @brief 觸發狀態查詢 (測試用)
 */
void cec_monitor_test_trigger_query(void) {
    ps5_power_state_t state = query_power_status();
    if (state != PS5_POWER_UNKNOWN && state != g_cec_ctx.current_state) {
        change_ps5_state(state);
    }
}

#endif // TESTING
