/**
 * @file server_state_machine.c
 * @brief Server State Machine Implementation (Platform Refactor Version)
 * 
 * 重構內容:
 * 1. 移除 led_controller 依賴
 * 2. ⭐ 使用 platform_set_led_state() 接口
 * 3. LED狀態映射到platform定義的狀態
 * 
 * @version 2.0.0
 * @date 2024-11-17
 */

#include "server_state_machine.h"

#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming-platform/platform_interface.h>
    #include <gaming/logger.h>
  #else
    #include "../../gaming-platform/src/platform_interface.h"
    #include "../../gaming-core/src/logger.h"
  #endif
#endif

#include <string.h>
#include <stdlib.h>

/* ============================================================
 *  Helper Functions
 * ============================================================ */

/**
 * @brief Server狀態轉換為Platform LED狀態
 */
static platform_led_state_t map_server_state_to_led(server_state_t state) {
    switch (state) {
        case SERVER_STATE_INIT:
            return LED_STATE_OFF;
        
        case SERVER_STATE_MONITORING:
            return LED_STATE_PS5_OFF;  // 監控中,PS5關閉
        
        case SERVER_STATE_PS5_DETECTED:
            return LED_STATE_PS5_ON;   // PS5開啟
        
        case SERVER_STATE_CLIENT_CONNECTED:
            return LED_STATE_VPN_CONNECTED;  // 客戶端已連線
        
        case SERVER_STATE_WAKING_PS5:
            return LED_STATE_WAKING;   // 正在喚醒PS5
        
        case SERVER_STATE_ERROR:
            return LED_STATE_ERROR;
        
        default:
            return LED_STATE_OFF;
    }
}

/**
 * @brief 更新LED顯示
 */
static void update_led_for_state(server_state_t state) {
#ifdef TESTING
    // 測試模式: 不實際控制LED
    (void)state;
#else
    // ⭐ 使用 platform 接口設定LED
    platform_led_state_t led_state = map_server_state_to_led(state);
    platform_set_led_state(led_state);
#endif
}

/**
 * @brief 狀態轉換為字串
 */
static const char* state_to_string(server_state_t state) {
    switch (state) {
        case SERVER_STATE_INIT:              return "INIT";
        case SERVER_STATE_MONITORING:        return "MONITORING";
        case SERVER_STATE_PS5_DETECTED:      return "PS5_DETECTED";
        case SERVER_STATE_CLIENT_CONNECTED:  return "CLIENT_CONNECTED";
        case SERVER_STATE_WAKING_PS5:        return "WAKING_PS5";
        case SERVER_STATE_ERROR:             return "ERROR";
        default:                             return "UNKNOWN";
    }
}

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

server_context_t* server_sm_create(const server_config_t *config) {
    if (config == NULL) {
        return NULL;
    }
    
    server_context_t *ctx = (server_context_t*)calloc(1, sizeof(server_context_t));
    if (ctx == NULL) {
        return NULL;
    }
    
    // 複製配置
    ctx->config = *config;
    
    // 初始化狀態
    ctx->current_state = SERVER_STATE_INIT;
    ctx->last_state = SERVER_STATE_INIT;
    ctx->error_count = 0;
    ctx->client_count = 0;
    ctx->ps5_power = PS5_POWER_UNKNOWN;
    ctx->ps5_network = PS5_NET_UNKNOWN;
    ctx->wake_requested = false;
    
    // 更新LED
    update_led_for_state(ctx->current_state);
    
    #ifndef TESTING
    logger_info("Server state machine created");
    #endif
    
    return ctx;
}

void server_sm_destroy(server_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    #ifndef TESTING
    logger_info("Server state machine destroyed");
    #endif
    
    free(ctx);
}

void server_sm_update(server_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    server_state_t next_state = ctx->current_state;
    
    // 狀態機邏輯
    switch (ctx->current_state) {
        case SERVER_STATE_INIT:
            // 初始化完成後,進入監控狀態
            next_state = SERVER_STATE_MONITORING;
            break;
        
        case SERVER_STATE_MONITORING:
            // PS5開機 -> 檢測到PS5
            if (ctx->ps5_power == PS5_POWER_ON) {
                next_state = SERVER_STATE_PS5_DETECTED;
            }
            // 有客戶端連線 -> 客戶端已連線
            else if (ctx->client_count > 0) {
                next_state = SERVER_STATE_CLIENT_CONNECTED;
            }
            // 錯誤累積過多
            else if (ctx->error_count > 5) {
                next_state = SERVER_STATE_ERROR;
            }
            break;
        
        case SERVER_STATE_PS5_DETECTED:
            // PS5關機 -> 返回監控
            if (ctx->ps5_power != PS5_POWER_ON) {
                next_state = SERVER_STATE_MONITORING;
            }
            // 有客戶端連線 -> 客戶端已連線
            else if (ctx->client_count > 0) {
                next_state = SERVER_STATE_CLIENT_CONNECTED;
            }
            break;
        
        case SERVER_STATE_CLIENT_CONNECTED:
            // 收到喚醒請求 -> 喚醒PS5
            if (ctx->wake_requested && ctx->ps5_power != PS5_POWER_ON) {
                next_state = SERVER_STATE_WAKING_PS5;
            }
            // 所有客戶端斷線 -> 返回適當狀態
            else if (ctx->client_count == 0) {
                if (ctx->ps5_power == PS5_POWER_ON) {
                    next_state = SERVER_STATE_PS5_DETECTED;
                } else {
                    next_state = SERVER_STATE_MONITORING;
                }
            }
            break;
        
        case SERVER_STATE_WAKING_PS5:
            // 喚醒完成 -> 返回客戶端連線狀態
            if (ctx->wake_completed) {
                next_state = SERVER_STATE_CLIENT_CONNECTED;
                ctx->wake_completed = false;
                ctx->wake_requested = false;
            }
            // 喚醒失敗
            else if (ctx->error_count > 3) {
                next_state = SERVER_STATE_ERROR;
            }
            break;
        
        case SERVER_STATE_ERROR:
            // 錯誤恢復: 重置狀態
            if (ctx->error_count == 0) {
                next_state = SERVER_STATE_INIT;
            }
            break;
        
        default:
            next_state = SERVER_STATE_ERROR;
            break;
    }
    
    // 狀態轉換
    if (next_state != ctx->current_state) {
        server_sm_transition(ctx, next_state);
    }
}

void server_sm_transition(server_context_t *ctx, server_state_t new_state) {
    if (ctx == NULL) {
        return;
    }
    
    server_state_t old_state = ctx->current_state;
    
    // 狀態轉換邏輯
    ctx->last_state = old_state;
    ctx->current_state = new_state;
    
    // ⭐ 更新LED顯示
    update_led_for_state(new_state);
    
    #ifndef TESTING
    logger_info("State transition: %s -> %s",
               state_to_string(old_state),
               state_to_string(new_state));
    #endif
    
    // 觸發進入回調
    if (ctx->on_state_enter) {
        ctx->on_state_enter(new_state, ctx->user_data);
    }
}

server_state_t server_sm_get_state(const server_context_t *ctx) {
    if (ctx == NULL) {
        return SERVER_STATE_ERROR;
    }
    return ctx->current_state;
}

void server_sm_on_ps5_power_changed(server_context_t *ctx, ps5_power_state_t power) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->ps5_power = power;
    
    #ifndef TESTING
    logger_info("PS5 power state changed: %s", ps5_power_state_to_string(power));
    #endif
}

void server_sm_on_ps5_network_changed(server_context_t *ctx, ps5_network_status_t status) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->ps5_network = status;
    
    #ifndef TESTING
    logger_info("PS5 network status changed: %s", 
               ps5_network_status_to_string(status));
    #endif
}

void server_sm_on_client_connected(server_context_t *ctx, int client_id) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->client_count++;
    
    #ifndef TESTING
    logger_info("Client %d connected (total: %d)", client_id, ctx->client_count);
    #endif
}

void server_sm_on_client_disconnected(server_context_t *ctx, int client_id) {
    if (ctx == NULL) {
        return;
    }
    
    if (ctx->client_count > 0) {
        ctx->client_count--;
    }
    
    #ifndef TESTING
    logger_info("Client %d disconnected (remaining: %d)", 
               client_id, ctx->client_count);
    #endif
}

void server_sm_on_wake_requested(server_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->wake_requested = true;
    
    #ifndef TESTING
    logger_info("PS5 wake requested");
    #endif
}

void server_sm_on_wake_completed(server_context_t *ctx, bool success) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->wake_completed = success;
    
    if (!success) {
        ctx->error_count++;
    }
    
    #ifndef TESTING
    logger_info("PS5 wake completed: %s", success ? "success" : "failed");
    #endif
}

void server_sm_on_error(server_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->error_count++;
    
    #ifndef TESTING
    logger_error("Error occurred (count: %d)", ctx->error_count);
    #endif
}

void server_sm_reset(server_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->current_state = SERVER_STATE_INIT;
    ctx->last_state = SERVER_STATE_INIT;
    ctx->error_count = 0;
    ctx->client_count = 0;
    ctx->ps5_power = PS5_POWER_UNKNOWN;
    ctx->ps5_network = PS5_NET_UNKNOWN;
    ctx->wake_requested = false;
    ctx->wake_completed = false;
    
    // 更新LED
    update_led_for_state(ctx->current_state);
    
    #ifndef TESTING
    logger_info("Server state machine reset");
    #endif
}

void server_sm_set_state_callback(server_context_t *ctx,
                                  server_state_callback_t callback,
                                  void *user_data)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->on_state_enter = callback;
    ctx->user_data = user_data;
}

/* ============================================================
 *  Utility Functions
 * ============================================================ */

const char* ps5_network_status_to_string(ps5_network_status_t status) {
    switch (status) {
        case PS5_NET_OFFLINE:   return "offline";
        case PS5_NET_ONLINE:    return "online";
        case PS5_NET_UNKNOWN:   return "unknown";
        default:                return "invalid";
    }
}
