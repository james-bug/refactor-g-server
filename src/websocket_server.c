/**
 * @file websocket_server.c
 * @brief WebSocket Server Implementation
 * 
 * 注意: 此為簡化版實作,用於測試環境
 * 生產環境應使用 libwebsockets 實作完整功能
 */

// POSIX headers for strncmp and other functions
#define _POSIX_C_SOURCE 200809L

#include "websocket_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h>

/* ============================================================
 *  Internal Structures
 * ============================================================ */

/**
 * @brief 客戶端連線結構
 */
typedef struct {
    int id;
    char ip[16];
    uint16_t port;
    time_t connect_time;
    bool active;
    void *ws_handle;  // libwebsockets wsi pointer (生產環境用)
} client_connection_t;

/**
 * @brief WebSocket Server 上下文
 */
typedef struct {
    // 配置
    int port;
    
    // 狀態
    ws_server_state_t state;
    bool initialized;
    
    // 客戶端管理
    client_connection_t clients[WS_SERVER_MAX_CLIENTS];
    int client_count;
    int next_client_id;
    
    // 回調
    ws_message_handler_t message_handler;
    void *message_handler_data;
    ws_connect_callback_t connect_callback;
    void *connect_callback_data;
    ws_disconnect_callback_t disconnect_callback;
    void *disconnect_callback_data;
    
} ws_server_context_t;

/* ============================================================
 *  Static Variables
 * ============================================================ */

static ws_server_context_t g_server_ctx = {0};

/* ============================================================
 *  Internal Helper Functions
 * ============================================================ */

/**
 * @brief 查找空閒客戶端槽位
 */
static int find_free_client_slot(void) {
    for (int i = 0; i < WS_SERVER_MAX_CLIENTS; i++) {
        if (!g_server_ctx.clients[i].active) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 查找客戶端索引
 */
static int find_client_by_id(int client_id) {
    for (int i = 0; i < WS_SERVER_MAX_CLIENTS; i++) {
        if (g_server_ctx.clients[i].active && 
            g_server_ctx.clients[i].id == client_id) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 解析 JSON 訊息類型
 */
static ws_message_type_t parse_message_type(const char *json_str) {
    if (json_str == NULL) {
        return WS_MSG_UNKNOWN;
    }
    
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        return WS_MSG_UNKNOWN;
    }
    
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        cJSON_Delete(root);
        return WS_MSG_UNKNOWN;
    }
    
    ws_message_type_t msg_type = WS_MSG_UNKNOWN;
    const char *type_str = type->valuestring;
    
    if (strncmp(type_str, "query_ps5", 9) == 0) {
        msg_type = WS_MSG_QUERY_PS5;
    } else if (strncmp(type_str, "wake_ps5", 8) == 0) {
        msg_type = WS_MSG_WAKE_PS5;
    } else if (strncmp(type_str, "ping", 4) == 0) {
        msg_type = WS_MSG_PING;
    } else if (strncmp(type_str, "pong", 4) == 0) {
        msg_type = WS_MSG_PONG;
    }
    
    cJSON_Delete(root);
    return msg_type;
}

/**
 * @brief 建立回應訊息 (預留給生產環境使用)
 */
__attribute__((unused))
static char* create_response(const char *type, const char *status, const char *message) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddStringToObject(root, "status", status);
    if (message) {
        cJSON_AddStringToObject(root, "message", message);
    }
    cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

/* ============================================================
 *  Public Function Implementations
 * ============================================================ */

/**
 * @brief 初始化 WebSocket Server
 */
int ws_server_init(int port) {
    if (g_server_ctx.initialized) {
        return -1;  // 已經初始化
    }
    
    // 初始化上下文
    memset(&g_server_ctx, 0, sizeof(ws_server_context_t));
    
    g_server_ctx.port = (port > 0) ? port : WS_SERVER_DEFAULT_PORT;
    g_server_ctx.state = WS_SERVER_STOPPED;
    g_server_ctx.initialized = true;
    g_server_ctx.next_client_id = 1;
    
    return 0;
}

/**
 * @brief 設定訊息處理回調
 */
void ws_server_set_message_handler(ws_message_handler_t handler, 
                                    void *user_data) {
    g_server_ctx.message_handler = handler;
    g_server_ctx.message_handler_data = user_data;
}

/**
 * @brief 設定連線回調
 */
void ws_server_set_connect_callback(ws_connect_callback_t handler,
                                     void *user_data) {
    g_server_ctx.connect_callback = handler;
    g_server_ctx.connect_callback_data = user_data;
}

/**
 * @brief 設定斷線回調
 */
void ws_server_set_disconnect_callback(ws_disconnect_callback_t handler,
                                        void *user_data) {
    g_server_ctx.disconnect_callback = handler;
    g_server_ctx.disconnect_callback_data = user_data;
}

/**
 * @brief 啟動 WebSocket Server
 */
int ws_server_start(void) {
    if (!g_server_ctx.initialized) {
        return -1;
    }
    
    if (g_server_ctx.state == WS_SERVER_RUNNING) {
        return 0;  // 已經在運行
    }
    
    g_server_ctx.state = WS_SERVER_STARTING;
    
#ifndef TESTING
    // 生產環境: 初始化 libwebsockets
    // TODO: 實作 libwebsockets 初始化
    // struct lws_context_creation_info info;
    // memset(&info, 0, sizeof(info));
    // info.port = g_server_ctx.port;
    // info.protocols = protocols;
    // g_server_ctx.lws_context = lws_create_context(&info);
#endif
    
    g_server_ctx.state = WS_SERVER_RUNNING;
    return 0;
}

/**
 * @brief 處理 WebSocket 事件 (非阻塞)
 */
int ws_server_service(int timeout_ms) {
    if (!g_server_ctx.initialized || g_server_ctx.state != WS_SERVER_RUNNING) {
        return -1;
    }
    
#ifdef TESTING
    // 測試模式: 什麼都不做
    (void)timeout_ms;
    return 0;
#else
    // 生產環境: 處理 libwebsockets 事件
    // return lws_service(g_server_ctx.lws_context, timeout_ms);
    return 0;
#endif
}

/**
 * @brief 廣播訊息給所有客戶端
 */
int ws_server_broadcast(const char *message) {
    if (!g_server_ctx.initialized || message == NULL) {
        return -1;
    }
    
    int sent_count = 0;
    
    for (int i = 0; i < WS_SERVER_MAX_CLIENTS; i++) {
        if (g_server_ctx.clients[i].active) {
            if (ws_server_send(g_server_ctx.clients[i].id, message) == 0) {
                sent_count++;
            }
        }
    }
    
    return sent_count;
}

/**
 * @brief 發送訊息給特定客戶端
 */
int ws_server_send(int client_id, const char *message) {
    if (!g_server_ctx.initialized || message == NULL) {
        return -1;
    }
    
    int client_idx = find_client_by_id(client_id);
    if (client_idx < 0) {
        return -2;  // 客戶端不存在
    }
    
#ifdef TESTING
    // 測試模式: 模擬發送成功
    return 0;
#else
    // 生產環境: 使用 libwebsockets 發送
    // client_connection_t *client = &g_server_ctx.clients[client_idx];
    // lws_write(client->ws_handle, (unsigned char*)message, strlen(message), LWS_WRITE_TEXT);
    return 0;
#endif
}

/**
 * @brief 取得連線的客戶端數量
 */
int ws_server_get_client_count(void) {
    return g_server_ctx.client_count;
}

/**
 * @brief 取得客戶端列表
 */
int ws_server_get_clients(ws_client_info_t *clients, int max_count) {
    if (clients == NULL || max_count <= 0) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < WS_SERVER_MAX_CLIENTS && count < max_count; i++) {
        if (g_server_ctx.clients[i].active) {
            clients[count].id = g_server_ctx.clients[i].id;
            snprintf(clients[count].ip, sizeof(clients[count].ip), 
                    "%s", g_server_ctx.clients[i].ip);
            clients[count].port = g_server_ctx.clients[i].port;
            clients[count].connect_time = g_server_ctx.clients[i].connect_time;
            clients[count].active = g_server_ctx.clients[i].active;
            count++;
        }
    }
    
    return count;
}

/**
 * @brief 取得伺服器狀態
 */
ws_server_state_t ws_server_get_state(void) {
    return g_server_ctx.state;
}

/**
 * @brief 取得監聽端口
 */
int ws_server_get_port(void) {
    if (!g_server_ctx.initialized) {
        return -1;
    }
    return g_server_ctx.port;
}

/**
 * @brief 停止 WebSocket Server
 */
void ws_server_stop(void) {
    if (!g_server_ctx.initialized) {
        return;
    }
    
    g_server_ctx.state = WS_SERVER_STOPPING;
    
#ifndef TESTING
    // 生產環境: 關閉 libwebsockets
    // if (g_server_ctx.lws_context) {
    //     lws_context_destroy(g_server_ctx.lws_context);
    //     g_server_ctx.lws_context = NULL;
    // }
#endif
    
    // 斷開所有客戶端
    for (int i = 0; i < WS_SERVER_MAX_CLIENTS; i++) {
        if (g_server_ctx.clients[i].active) {
            if (g_server_ctx.disconnect_callback) {
                g_server_ctx.disconnect_callback(
                    g_server_ctx.clients[i].id,
                    g_server_ctx.disconnect_callback_data
                );
            }
            g_server_ctx.clients[i].active = false;
        }
    }
    
    g_server_ctx.client_count = 0;
    g_server_ctx.state = WS_SERVER_STOPPED;
}

/**
 * @brief 清理資源
 */
void ws_server_cleanup(void) {
    if (g_server_ctx.state == WS_SERVER_RUNNING) {
        ws_server_stop();
    }
    
    memset(&g_server_ctx, 0, sizeof(ws_server_context_t));
}

/**
 * @brief 訊息類型轉換為字串
 */
const char* ws_message_type_to_string(ws_message_type_t msg_type) {
    switch (msg_type) {
        case WS_MSG_UNKNOWN:    return "unknown";
        case WS_MSG_QUERY_PS5:  return "query_ps5";
        case WS_MSG_WAKE_PS5:   return "wake_ps5";
        case WS_MSG_PING:       return "ping";
        case WS_MSG_PONG:       return "pong";
        default:                return "invalid";
    }
}

/**
 * @brief 伺服器狀態轉換為字串
 */
const char* ws_server_state_to_string(ws_server_state_t state) {
    switch (state) {
        case WS_SERVER_STOPPED:  return "STOPPED";
        case WS_SERVER_STARTING: return "STARTING";
        case WS_SERVER_RUNNING:  return "RUNNING";
        case WS_SERVER_STOPPING: return "STOPPING";
        case WS_SERVER_ERROR:    return "ERROR";
        default:                 return "UNKNOWN";
    }
}

/**
 * @brief 錯誤碼轉換為字串
 */
const char* ws_server_error_string(int error) {
    switch (error) {
        case 0:  return "Success";
        case -1: return "Not initialized or invalid parameters";
        case -2: return "Client not found";
        case -3: return "Server not running";
        case -4: return "Max clients reached";
        default: return "Unknown error";
    }
}

/* ============================================================
 *  測試輔助函數 (僅供測試使用)
 * ============================================================ */

#ifdef TESTING

/**
 * @brief 模擬客戶端連線 (測試用)
 */
int ws_server_test_add_client(const char *ip, uint16_t port) {
    if (!g_server_ctx.initialized) {
        return -1;
    }
    
    int slot = find_free_client_slot();
    if (slot < 0) {
        return -4;  // 達到最大客戶端數
    }
    
    int client_id = g_server_ctx.next_client_id++;
    
    g_server_ctx.clients[slot].id = client_id;
    snprintf(g_server_ctx.clients[slot].ip, sizeof(g_server_ctx.clients[slot].ip), 
            "%s", ip);
    g_server_ctx.clients[slot].port = port;
    g_server_ctx.clients[slot].connect_time = time(NULL);
    g_server_ctx.clients[slot].active = true;
    g_server_ctx.client_count++;
    
    // 觸發連線回調
    if (g_server_ctx.connect_callback) {
        g_server_ctx.connect_callback(client_id, ip, 
                                      g_server_ctx.connect_callback_data);
    }
    
    return client_id;
}

/**
 * @brief 模擬客戶端斷線 (測試用)
 */
int ws_server_test_remove_client(int client_id) {
    int client_idx = find_client_by_id(client_id);
    if (client_idx < 0) {
        return -2;
    }
    
    g_server_ctx.clients[client_idx].active = false;
    g_server_ctx.client_count--;
    
    // 觸發斷線回調
    if (g_server_ctx.disconnect_callback) {
        g_server_ctx.disconnect_callback(client_id, 
                                         g_server_ctx.disconnect_callback_data);
    }
    
    return 0;
}

/**
 * @brief 模擬接收訊息 (測試用)
 */
char* ws_server_test_handle_message(int client_id, const char *message) {
    if (!g_server_ctx.message_handler) {
        return NULL;
    }
    
    ws_message_type_t msg_type = parse_message_type(message);
    
    return g_server_ctx.message_handler(client_id, msg_type, message,
                                        g_server_ctx.message_handler_data);
}

#endif // TESTING
