/**
 * @file cec_monitor.h
 * @brief CEC Monitor - PS5 Power State Monitoring via HDMI-CEC (Platform Refactor Version)
 * 
 * 重構內容:
 * 1. 簡化回調函數簽名 - 移除 event 參數
 * 2. 使用 platform_interface.h 的 PS5 狀態查詢
 * 3. 保持向後兼容的 API
 * 
 * @author Gaming System Development Team
 * @date 2025-11-18
 * @version 2.0.0
 */

#ifndef CEC_MONITOR_H
#define CEC_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  Constants
 * ============================================================ */

#define CEC_OK                      0
#define CEC_ERROR_NOT_INIT         -1
#define CEC_ERROR_DEVICE_NOT_FOUND -2
#define CEC_ERROR_OPEN_FAILED      -3
#define CEC_ERROR_COMMAND_FAILED   -4

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief PS5 power states
 */
typedef enum {
    PS5_POWER_UNKNOWN = 0,      /**< Unknown state */
    PS5_POWER_OFF,              /**< Power off */
    PS5_POWER_STANDBY,          /**< Standby mode */
    PS5_POWER_ON,               /**< Powered on */
} ps5_power_state_t;

/**
 * @brief PS5 state change callback (簡化版本)
 * 
 * ⭐ 平台重構: 移除 event 參數，只保留 state
 * 
 * @param state New PS5 power state
 * @param user_data User-provided data pointer
 */
typedef void (*ps5_state_callback_t)(ps5_power_state_t state, void *user_data);

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Initialize CEC monitor
 * 
 * @return CEC_OK on success, negative error code on failure
 */
int cec_monitor_init(void);

/**
 * @brief Clean up CEC monitor resources
 */
void cec_monitor_cleanup(void);

/**
 * @brief Start monitoring PS5 power state
 * 
 * @return CEC_OK on success, negative error code on failure
 */
int cec_monitor_start(void);

/**
 * @brief Stop monitoring
 */
void cec_monitor_stop(void);

/**
 * @brief Get current PS5 power state (from cache)
 * 
 * @return Current PS5 power state
 */
ps5_power_state_t cec_monitor_get_state(void);

/**
 * @brief Get last update timestamp
 * 
 * @return Timestamp of last state update
 */
time_t cec_monitor_get_last_update_time(void);

/**
 * @brief Set state change callback
 * 
 * @param callback Callback function
 * @param user_data User data to pass to callback
 */
void cec_monitor_set_callback(ps5_state_callback_t callback, void *user_data);

/**
 * @brief Convert PS5 power state to string
 * 
 * @param state PS5 power state
 * @return String representation
 */
const char* ps5_power_state_to_string(ps5_power_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* CEC_MONITOR_H */
