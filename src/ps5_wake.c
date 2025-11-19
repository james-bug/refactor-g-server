/**
 * @file ps5_wake.c
 * @brief PS5 Wake Controller Implementation (Platform Refactor Version)
 * 
 * 重構內容:
 * 1. 移除直接的 cec-ctl 調用
 * 2. ⭐ 使用 platform_send_ps5_wake() 接口
 * 3. 簡化喚醒邏輯
 * 
 * @version 2.0.1
 * @date 2024-11-18
 */

#include "ps5_wake.h"

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
#include <unistd.h>
#include <time.h>

/* ============================================================
 *  Constants
 * ============================================================ */

#define WAKE_VERIFY_DELAY_MS    3000    // 喚醒後等待3秒驗證
#define WAKE_MAX_RETRIES        3       // 最大重試次數

/* ============================================================
 *  Type Definitions
 * ============================================================ */

typedef struct {
    bool initialized;
    int retry_count;
    time_t last_wake_time;
    
    // 回調函數
    ps5_wake_callback_t wake_callback;
    void *callback_data;
    
} ps5_wake_context_t;

/* ============================================================
 *  Global Variables
 * ============================================================ */

static ps5_wake_context_t g_wake_ctx = {0};

/* ============================================================
 *  Helper Functions
 * ============================================================ */

/**
 * @brief 執行喚醒命令
 * @return 0=成功, -1=失敗
 */
static int execute_wake_command(void) {
#ifdef TESTING
    // 測試模式: 模擬喚醒成功
    return 0;
#else
    // ⭐ 使用 platform 接口發送喚醒命令
    int result = platform_send_ps5_wake();
    
    if (result == PLATFORM_OK) {
        logger_info("PS5 wake command sent successfully");
        return 0;
    } else {
        logger_error("Failed to send PS5 wake command: %d", result);
        return -1;
    }
#endif
}

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

int ps5_wake_init(void) {
    if (g_wake_ctx.initialized) {
        #ifndef TESTING
        logger_warning("PS5 wake controller already initialized");
        #endif
        return 0;
    }
    
    // 初始化context
    memset(&g_wake_ctx, 0, sizeof(g_wake_ctx));
    g_wake_ctx.retry_count = 0;
    g_wake_ctx.initialized = true;
    
    #ifndef TESTING
    logger_info("PS5 wake controller initialized (using platform interface)");
    #endif
    
    return 0;
}

void ps5_wake_cleanup(void) {
    if (!g_wake_ctx.initialized) {
        return;
    }
    
    memset(&g_wake_ctx, 0, sizeof(g_wake_ctx));
    g_wake_ctx.initialized = false;
    
    #ifndef TESTING
    logger_info("PS5 wake controller cleaned up");
    #endif
}

int ps5_wake_send(void) {
    if (!g_wake_ctx.initialized) {
        return -1;
    }
    
    #ifndef TESTING
    logger_info("Attempting to wake PS5...");
    #endif
    
    int retry = 0;
    int result = -1;
    
    while (retry < WAKE_MAX_RETRIES) {
        // 執行喚醒命令
        result = execute_wake_command();
        
        if (result == 0) {
            // 喚醒成功
            g_wake_ctx.last_wake_time = time(NULL);
            g_wake_ctx.retry_count = 0;
            
            #ifndef TESTING
            logger_info("PS5 wake command sent successfully");
            #endif
            
            // 觸發成功回調
            if (g_wake_ctx.wake_callback) {
                g_wake_ctx.wake_callback(true, g_wake_ctx.callback_data);
            }
            
            return 0;
        }
        
        // 重試
        retry++;
        g_wake_ctx.retry_count = retry;
        
        #ifndef TESTING
        logger_warning("PS5 wake attempt %d/%d failed, retrying...", 
                   retry, WAKE_MAX_RETRIES);
        #endif
        
        if (retry < WAKE_MAX_RETRIES) {
            usleep(1000000);  // 等待1秒後重試
        }
    }
    
    // 喚醒失敗
    #ifndef TESTING
    logger_error("Failed to wake PS5 after %d attempts", WAKE_MAX_RETRIES);
    #endif
    
    // 觸發失敗回調
    if (g_wake_ctx.wake_callback) {
        g_wake_ctx.wake_callback(false, g_wake_ctx.callback_data);
    }
    
    return -1;
}

int ps5_wake_verify(ps5_power_state_t *state) {
    if (!g_wake_ctx.initialized || state == NULL) {
        return -1;
    }
    
    // 等待PS5啟動
    #ifndef TESTING
    logger_info("Waiting %d seconds to verify PS5 power state...", 
               WAKE_VERIFY_DELAY_MS / 1000);
    #endif
    
    usleep(WAKE_VERIFY_DELAY_MS * 1000);
    
    // 查詢電源狀態
#ifdef TESTING
    // 測試模式: 返回預設狀態
    *state = PS5_POWER_ON;
    return 0;
#else
    // 使用 platform 接口查詢狀態
    platform_ps5_power_t power = platform_get_ps5_power();
    
    switch (power) {
        case PLATFORM_PS5_ON:
            *state = PS5_POWER_ON;
            logger_info("PS5 verified as powered ON");
            return 0;
        
        case PLATFORM_PS5_STANDBY:
            *state = PS5_POWER_STANDBY;
            logger_warning("PS5 in STANDBY mode");
            return 0;
        
        case PLATFORM_PS5_OFF:
            *state = PS5_POWER_OFF;
            logger_error("PS5 still OFF after wake attempt");
            return -1;
        
        default:
            *state = PS5_POWER_UNKNOWN;
            logger_error("Unable to verify PS5 power state");
            return -1;
    }
#endif
}

time_t ps5_wake_get_last_time(void) {
    return g_wake_ctx.last_wake_time;
}

int ps5_wake_get_retry_count(void) {
    return g_wake_ctx.retry_count;
}

void ps5_wake_set_callback(ps5_wake_callback_t callback, void *user_data) {
    g_wake_ctx.wake_callback = callback;
    g_wake_ctx.callback_data = user_data;
}

/* ============================================================
 *  測試輔助函數 (僅供測試使用)
 * ============================================================ */

#ifdef TESTING

/**
 * @brief 模擬喚醒結果 (測試用)
 */
void ps5_wake_test_set_result(bool success) {
    if (g_wake_ctx.wake_callback) {
        g_wake_ctx.wake_callback(success, g_wake_ctx.callback_data);
    }
}

/**
 * @brief 重置重試計數 (測試用)
 */
void ps5_wake_test_reset_retry_count(void) {
    g_wake_ctx.retry_count = 0;
}

#endif // TESTING
