/**
 * @file server_state_machine.h
 * @brief Server State Machine - Gaming Server State Management (Platform Refactor Version)
 * 
 * 重構內容:
 * 1. 使用 platform_interface.h 的 LED 控制
 * 2. 移除直接的 led_controller 依賴
 * 3. 保持狀態機邏輯不變
 * 
 * @author Gaming System Development Team
 * @date 2025-11-18
 * @version 2.0.0
 */

#ifndef SERVER_STATE_MACHINE_H
#define SERVER_STATE_MACHINE_H

#include <stdbool.h>
#include "cec_monitor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief Server states
 */
typedef enum {
    SERVER_STATE_INIT = 0,          /**< Initialization */
    SERVER_STATE_MONITORING,        /**< Monitoring PS5 state */
    SERVER_STATE_PS5_DETECTED,      /**< PS5 is powered on */
    SERVER_STATE_CLIENT_CONNECTED,  /**< Client connected */
    SERVER_STATE_WAKING_PS5,        /**< Waking PS5 */
    SERVER_STATE_ERROR,             /**< Error state */
} server_state_t;

/**
 * @brief PS5 network status
 */
typedef enum {
    PS5_NET_UNKNOWN = 0,            /**< Unknown */
    PS5_NET_OFFLINE,                /**< Offline */
    PS5_NET_ONLINE,                 /**< Online */
} ps5_network_status_t;

/**
 * @brief Server configuration
 */
typedef struct {
    int ws_port;                    /**< WebSocket port */
    char ps5_subnet[32];            /**< PS5 subnet for detection */
    char cache_path[256];           /**< Cache file path */
} server_config_t;

/**
 * @brief Server state machine context
 */
typedef struct {
    server_config_t config;         /**< Configuration */
    server_state_t current_state;   /**< Current state */
    server_state_t last_state;      /**< Last state */
    
    // PS5 status
    ps5_power_state_t ps5_power;    /**< PS5 power state */
    ps5_network_status_t ps5_network; /**< PS5 network status */
    
    // Client status
    int client_count;               /**< Connected client count */
    
    // Flags
    bool wake_requested;            /**< Wake request pending */
    bool wake_completed;            /**< Wake completed flag */
    int error_count;                /**< Error counter */
    
    // Callbacks
    void (*on_state_enter)(server_state_t state, void *user_data);
    void *user_data;
} server_context_t;

/**
 * @brief State enter callback
 * 
 * @param state New state entered
 * @param user_data User-provided data pointer
 */
typedef void (*server_state_callback_t)(server_state_t state, void *user_data);

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Create server state machine
 * 
 * @param config Server configuration
 * @return State machine context, NULL on failure
 */
server_context_t* server_sm_create(const server_config_t *config);

/**
 * @brief Destroy server state machine
 * 
 * @param ctx State machine context
 */
void server_sm_destroy(server_context_t *ctx);

/**
 * @brief Update state machine (call in main loop)
 * 
 * @param ctx State machine context
 */
void server_sm_update(server_context_t *ctx);

/**
 * @brief Transition to new state
 * 
 * @param ctx State machine context
 * @param new_state Target state
 */
void server_sm_transition(server_context_t *ctx, server_state_t new_state);

/**
 * @brief Get current state
 * 
 * @param ctx State machine context
 * @return Current state
 */
server_state_t server_sm_get_state(const server_context_t *ctx);

/**
 * @brief PS5 power state changed event
 * 
 * @param ctx State machine context
 * @param power New PS5 power state
 */
void server_sm_on_ps5_power_changed(server_context_t *ctx, ps5_power_state_t power);

/**
 * @brief PS5 network status changed event
 * 
 * @param ctx State machine context
 * @param status New network status
 */
void server_sm_on_ps5_network_changed(server_context_t *ctx, ps5_network_status_t status);

/**
 * @brief Client connected event
 * 
 * @param ctx State machine context
 * @param client_id Client ID
 */
void server_sm_on_client_connected(server_context_t *ctx, int client_id);

/**
 * @brief Client disconnected event
 * 
 * @param ctx State machine context
 * @param client_id Client ID
 */
void server_sm_on_client_disconnected(server_context_t *ctx, int client_id);

/**
 * @brief Wake PS5 requested event
 * 
 * @param ctx State machine context
 */
void server_sm_on_wake_requested(server_context_t *ctx);

/**
 * @brief Wake PS5 completed event
 * 
 * @param ctx State machine context
 * @param success true if wake succeeded
 */
void server_sm_on_wake_completed(server_context_t *ctx, bool success);

/**
 * @brief Error occurred event
 * 
 * @param ctx State machine context
 */
void server_sm_on_error(server_context_t *ctx);

/**
 * @brief Reset state machine
 * 
 * @param ctx State machine context
 */
void server_sm_reset(server_context_t *ctx);

/**
 * @brief Set state enter callback
 * 
 * @param ctx State machine context
 * @param callback Callback function
 * @param user_data User data for callback
 */
void server_sm_set_state_callback(server_context_t *ctx,
                                  server_state_callback_t callback,
                                  void *user_data);

/**
 * @brief Convert network status to string
 * 
 * @param status Network status
 * @return String representation
 */
const char* ps5_network_status_to_string(ps5_network_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* SERVER_STATE_MACHINE_H */
