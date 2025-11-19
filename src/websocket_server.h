/**
 * @file websocket_server.h
 * @brief WebSocket Server Module - 提供 WebSocket 服務給 gaming-client
 * 
 * 此模組提供 WebSocket 伺服器功能:
 * - 監聽客戶端連線
 * - 處理多個客戶端
 * - 接收並回應 JSON 訊息
 * - 廣播狀態變更
 * 
 * @author Gaming System Development Team
 * @date 2025-11-05
 * @version 1.0.0
 */

#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup WebSocketServer WebSocket Server Module
 * @brief WebSocket server for client communication
 * @{
 */

/* ============================================================
 *  Constants and Macros
 * ============================================================ */

/** 預設 WebSocket 端口 */
#define WS_SERVER_DEFAULT_PORT          8080

/** 最大客戶端連線數 */
#define WS_SERVER_MAX_CLIENTS           10

/** 最大訊息大小 (bytes) */
#define WS_SERVER_MAX_MESSAGE_SIZE      4096

/** Ping 間隔 (毫秒) */
#define WS_SERVER_PING_INTERVAL_MS      30000

/** Pong 超時 (毫秒) */
#define WS_SERVER_PONG_TIMEOUT_MS       5000

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief WebSocket Server 狀態
 */
typedef enum {
    WS_SERVER_STOPPED = 0,      /**< 已停止 */
    WS_SERVER_STARTING,         /**< 啟動中 */
    WS_SERVER_RUNNING,          /**< 運行中 */
    WS_SERVER_STOPPING,         /**< 停止中 */
    WS_SERVER_ERROR,            /**< 錯誤狀態 */
} ws_server_state_t;

/**
 * @brief 客戶端資訊
 */
typedef struct {
    int id;                     /**< 客戶端 ID */
    char ip[16];                /**< IP 位址 */
    uint16_t port;              /**< 端口 */
    time_t connect_time;        /**< 連線時間 */
    bool active;                /**< 是否活躍 */
} ws_client_info_t;

/**
 * @brief WebSocket 訊息類型
 */
typedef enum {
    WS_MSG_UNKNOWN = 0,         /**< 未知訊息 */
    WS_MSG_QUERY_PS5,           /**< 查詢 PS5 狀態 */
    WS_MSG_WAKE_PS5,            /**< 喚醒 PS5 */
    WS_MSG_PING,                /**< Ping */
    WS_MSG_PONG,                /**< Pong */
} ws_message_type_t;

/**
 * @brief 訊息處理回調函數類型
 * 
 * @param client_id 客戶端 ID
 * @param msg_type 訊息類型
 * @param payload 訊息內容 (JSON 字串)
 * @param user_data 使用者資料
 * @return 回應訊息 (JSON 字串,需由呼叫者使用 free() 釋放), NULL 表示不回應
 */
typedef char* (*ws_message_handler_t)(int client_id, 
                                       ws_message_type_t msg_type,
                                       const char *payload, 
                                       void *user_data);

/**
 * @brief 客戶端連線回調函數類型
 * 
 * @param client_id 客戶端 ID
 * @param client_ip 客戶端 IP
 * @param user_data 使用者資料
 */
typedef void (*ws_connect_callback_t)(int client_id, 
                                       const char *client_ip,
                                       void *user_data);

/**
 * @brief 客戶端斷線回調函數類型
 * 
 * @param client_id 客戶端 ID
 * @param user_data 使用者資料
 */
typedef void (*ws_disconnect_callback_t)(int client_id, 
                                          void *user_data);

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief 初始化 WebSocket Server
 * 
 * @param port 監聽端口 (0 使用預設端口)
 * @return 0 成功, <0 失敗
 */
int ws_server_init(int port);

/**
 * @brief 設定訊息處理回調
 * 
 * @param handler 訊息處理函數
 * @param user_data 使用者資料
 */
void ws_server_set_message_handler(ws_message_handler_t handler, 
                                    void *user_data);

/**
 * @brief 設定連線回調
 * 
 * @param handler 連線回調函數
 * @param user_data 使用者資料
 */
void ws_server_set_connect_callback(ws_connect_callback_t handler,
                                     void *user_data);

/**
 * @brief 設定斷線回調
 * 
 * @param handler 斷線回調函數
 * @param user_data 使用者資料
 */
void ws_server_set_disconnect_callback(ws_disconnect_callback_t handler,
                                        void *user_data);

/**
 * @brief 啟動 WebSocket Server
 * 
 * @return 0 成功, <0 失敗
 */
int ws_server_start(void);

/**
 * @brief 處理 WebSocket 事件 (非阻塞)
 * 
 * 此函數應該在主循環中定期呼叫
 * 
 * @param timeout_ms 超時時間 (毫秒), 0 為立即返回
 * @return 0 成功, <0 失敗
 */
int ws_server_service(int timeout_ms);

/**
 * @brief 廣播訊息給所有客戶端
 * 
 * @param message 訊息內容 (JSON 字串)
 * @return 成功發送的客戶端數量, <0 失敗
 */
int ws_server_broadcast(const char *message);

/**
 * @brief 發送訊息給特定客戶端
 * 
 * @param client_id 客戶端 ID
 * @param message 訊息內容 (JSON 字串)
 * @return 0 成功, <0 失敗
 */
int ws_server_send(int client_id, const char *message);

/**
 * @brief 取得連線的客戶端數量
 * 
 * @return 客戶端數量
 */
int ws_server_get_client_count(void);

/**
 * @brief 取得客戶端列表
 * 
 * @param clients 客戶端資訊陣列 (由呼叫者提供)
 * @param max_count 陣列最大容量
 * @return 實際客戶端數量
 */
int ws_server_get_clients(ws_client_info_t *clients, int max_count);

/**
 * @brief 取得伺服器狀態
 * 
 * @return 伺服器狀態
 */
ws_server_state_t ws_server_get_state(void);

/**
 * @brief 取得監聽端口
 * 
 * @return 端口號, <0 表示未啟動
 */
int ws_server_get_port(void);

/**
 * @brief 停止 WebSocket Server
 */
void ws_server_stop(void);

/**
 * @brief 清理資源
 */
void ws_server_cleanup(void);

/**
 * @brief 訊息類型轉換為字串
 * 
 * @param msg_type 訊息類型
 * @return 訊息類型字串
 */
const char* ws_message_type_to_string(ws_message_type_t msg_type);

/**
 * @brief 伺服器狀態轉換為字串
 * 
 * @param state 伺服器狀態
 * @return 狀態字串
 */
const char* ws_server_state_to_string(ws_server_state_t state);

/**
 * @brief 錯誤碼轉換為字串
 * 
 * @param error 錯誤碼
 * @return 錯誤訊息字串
 */
const char* ws_server_error_string(int error);

/** @} */ // end of WebSocketServer group

#ifdef __cplusplus
}
#endif

#endif // WEBSOCKET_SERVER_H
