/**
 * @file ps5_wake.h
 * @brief PS5 Wake Controller - Wake PS5 via HDMI-CEC (Platform Refactor Version)
 * 
 * 重構內容:
 * 1. 使用 platform_interface.h 的 PS5 喚醒接口
 * 2. 簡化回調函數簽名
 * 3. 移除直接的 cec-ctl 調用
 * 
 * @author Gaming System Development Team
 * @date 2025-11-18
 * @version 2.0.0
 */

#ifndef PS5_WAKE_H
#define PS5_WAKE_H

#include <stdbool.h>
#include <time.h>
#include "cec_monitor.h"  // For ps5_power_state_t

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief PS5 wake callback
 * 
 * @param success true if wake succeeded, false if failed
 * @param user_data User-provided data pointer
 */
typedef void (*ps5_wake_callback_t)(bool success, void *user_data);

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Initialize PS5 wake controller
 * 
 * @return 0 on success, negative error code on failure
 */
int ps5_wake_init(void);

/**
 * @brief Clean up PS5 wake controller
 */
void ps5_wake_cleanup(void);

/**
 * @brief Send wake command to PS5
 * 
 * This function sends a CEC wake command to PS5.
 * Use ps5_wake_verify() to check if PS5 actually powered on.
 * 
 * @return 0 on success, negative error code on failure
 */
int ps5_wake_send(void);

/**
 * @brief Verify PS5 power state after wake
 * 
 * This function waits for PS5 to power on and verifies the state.
 * 
 * @param state Pointer to store verified power state
 * @return 0 if PS5 is ON, negative error code otherwise
 */
int ps5_wake_verify(ps5_power_state_t *state);

/**
 * @brief Get timestamp of last wake attempt
 * 
 * @return Timestamp of last wake command
 */
time_t ps5_wake_get_last_time(void);

/**
 * @brief Get retry count
 * 
 * @return Number of retries attempted
 */
int ps5_wake_get_retry_count(void);

/**
 * @brief Set wake completion callback
 * 
 * @param callback Callback function
 * @param user_data User data to pass to callback
 */
void ps5_wake_set_callback(ps5_wake_callback_t callback, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* PS5_WAKE_H */
