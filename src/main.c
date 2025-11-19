/**
 * @file main.c
 * @brief Gaming Server Main Daemon (Platform Refactor Version)
 * 
 * 重構內容:
 * 1. ⭐ 添加 platform_init() 在最開始
 * 2. ⭐ 驗證設備類型必須為 "server"
 * 3. ⭐ 添加 platform_cleanup() 在結束時
 * 4. 移除直接的硬體初始化(由platform處理)
 * 
 * @version 2.0.0
 * @date 2024-11-17
 */

#include "server_state_machine.h"
#include "cec_monitor.h"
#include "ps5_wake.h"
#include "ps5_detector.h"
#include "websocket_server.h"

#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming-platform/platform_interface.h>
    #include <gaming/logger.h>
    #include <gaming/config_parser.h>
  #else
    #include "../../gaming-platform/src/platform_interface.h"
    #include "../../gaming-core/src/logger.h"
    #include "../../gaming-core/src/config_parser.h"
  #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

/* ============================================================
 *  Constants
 * ============================================================ */

#define PROGRAM_NAME    "gaming-server"
#define PROGRAM_VERSION "2.0.0"

// Default configuration values
#define DEFAULT_WS_PORT             8080
#define DEFAULT_PS5_SUBNET          "192.168.1.0/24"
#define DEFAULT_CACHE_PATH          "/var/run/gaming/ps5_cache.json"

/* ============================================================
 *  Global Variables
 * ============================================================ */

static volatile sig_atomic_t g_running = 1;
static server_context_t *g_server_ctx = NULL;

/* ============================================================
 *  Signal Handlers
 * ============================================================ */

static void signal_handler(int signum) {
    switch (signum) {
        case SIGINT:
        case SIGTERM:
            #ifndef TESTING
            logger_info("Received signal %d, shutting down...", signum);
            #endif
            g_running = 0;
            break;
        
        case SIGHUP:
            #ifndef TESTING
            logger_info("Received SIGHUP, reloading configuration...");
            #endif
            // TODO: 重新載入配置
            break;
        
        default:
            break;
    }
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    // 忽略SIGPIPE
    signal(SIGPIPE, SIG_IGN);
}

/* ============================================================
 *  Callback Functions
 * ============================================================ */

/**
 * @brief CEC監控回調: PS5電源狀態改變
 */
static void on_ps5_power_changed(ps5_power_state_t state, void *user_data) {
    (void)user_data;
    
    if (g_server_ctx) {
        server_sm_on_ps5_power_changed(g_server_ctx, state);
    }
    
    // 同時通知所有連線的客戶端
    char message[128];
    snprintf(message, sizeof(message), 
            "{\"type\":\"ps5_status\",\"power\":\"%s\"}",
            ps5_power_state_to_string(state));
    ws_server_broadcast(message);
}

/**
 * @brief PS5喚醒回調
 */
static void on_ps5_wake_completed(bool success, void *user_data) {
    (void)user_data;
    
    if (g_server_ctx) {
        server_sm_on_wake_completed(g_server_ctx, success);
    }
}

/**
 * @brief WebSocket客戶端連線回調
 */
static void on_client_connected(int client_id, const char *client_ip, void *user_data) {
    (void)user_data;
    
    #ifndef TESTING
    logger_info("Client %d connected from %s", client_id, client_ip);
    #endif
    
    if (g_server_ctx) {
        server_sm_on_client_connected(g_server_ctx, client_id);
    }
    
    // 發送當前PS5狀態給新連線的客戶端
    ps5_power_state_t power = cec_monitor_get_state();
    char message[128];
    snprintf(message, sizeof(message),
            "{\"type\":\"ps5_status\",\"power\":\"%s\"}",
            ps5_power_state_to_string(power));
    ws_server_send(client_id, message);
}

/**
 * @brief WebSocket客戶端斷線回調
 */
static void on_client_disconnected(int client_id, void *user_data) {
    (void)user_data;
    
    #ifndef TESTING
    logger_info("Client %d disconnected", client_id);
    #endif
    
    if (g_server_ctx) {
        server_sm_on_client_disconnected(g_server_ctx, client_id);
    }
}

/**
 * @brief WebSocket訊息處理器
 */
static char* handle_client_message(int client_id, ws_message_type_t msg_type,
                                   const char *message, void *user_data)
{
    (void)user_data;
    
    #ifndef TESTING
    logger_debug("Received message from client %d: %s", client_id, message);
    #endif
    
    char *response = NULL;
    
    switch (msg_type) {
        case WS_MSG_QUERY_PS5: {
            // 查詢PS5電源狀態
            ps5_power_state_t power = cec_monitor_get_state();
            
            // 查詢PS5網路狀態
            ps5_info_t ps5_info = {0};
            int detect_result = ps5_detector_get_cached(&ps5_info);
            bool ps5_online = (detect_result == PS5_DETECT_OK && ps5_info.online);
            
            response = (char*)malloc(256);
            if (response) {
                snprintf(response, 256,
                        "{\"type\":\"ps5_status\",\"power\":\"%s\",\"network\":\"%s\"}",
                        ps5_power_state_to_string(power),
                        ps5_online ? "online" : "offline");
            }
            break;
        }
        
        case WS_MSG_WAKE_PS5: {
            // 喚醒PS5
            #ifndef TESTING
            logger_info("Client %d requested PS5 wake", client_id);
            #endif
            
            if (g_server_ctx) {
                server_sm_on_wake_requested(g_server_ctx);
            }
            
            // 執行喚醒
            int result = ps5_wake_send();
            
            response = (char*)malloc(128);
            if (response) {
                snprintf(response, 128,
                        "{\"type\":\"wake_result\",\"success\":%s}",
                        result == 0 ? "true" : "false");
            }
            break;
        }
        
        case WS_MSG_PING: {
            // Ping回應
            response = strdup("{\"type\":\"pong\"}");
            break;
        }
        
        default:
            #ifndef TESTING
            logger_warning("Unknown message type from client %d", client_id);
            #endif
            break;
    }
    
    return response;
}

/**
 * @brief 狀態機狀態進入回調
 */
static void on_state_enter(server_state_t state, void *user_data) {
    (void)user_data;
    
    // 根據狀態執行相應動作
    switch (state) {
        case SERVER_STATE_MONITORING:
            // 開始CEC監控
            cec_monitor_start();
            break;
        
        case SERVER_STATE_WAKING_PS5:
            // 執行喚醒
            ps5_wake_send();
            break;
        
        default:
            break;
    }
}

/* ============================================================
 *  Initialization & Cleanup
 * ============================================================ */

/**
 * @brief 初始化所有模組
 */
static int initialize_modules(const server_config_t *config) {
    int ret;
    
    // 1. 初始化CEC Monitor
    ret = cec_monitor_init();
    if (ret != 0) {
        #ifndef TESTING
        logger_error("Failed to initialize CEC monitor");
        #endif
        return -1;
    }
    cec_monitor_set_callback(on_ps5_power_changed, NULL);
    
    // 2. 初始化PS5 Wake Controller
    ret = ps5_wake_init();
    if (ret != 0) {
        #ifndef TESTING
        logger_error("Failed to initialize PS5 wake controller");
        #endif
        cec_monitor_cleanup();
        return -1;
    }
    ps5_wake_set_callback(on_ps5_wake_completed, NULL);
    
    // 3. 初始化PS5 Detector
    ret = ps5_detector_init(config->ps5_subnet, config->cache_path);
    if (ret != 0) {
        #ifndef TESTING
        logger_error("Failed to initialize PS5 detector");
        #endif
        ps5_wake_cleanup();
        cec_monitor_cleanup();
        return -1;
    }
    
    // 4. 初始化WebSocket Server
    ret = ws_server_init(config->ws_port);
    if (ret != 0) {
        #ifndef TESTING
        logger_error("Failed to initialize WebSocket server");
        #endif
        ps5_detector_cleanup();
        ps5_wake_cleanup();
        cec_monitor_cleanup();
        return -1;
    }
    
    ws_server_set_connect_callback(on_client_connected, NULL);
    ws_server_set_disconnect_callback(on_client_disconnected, NULL);
    ws_server_set_message_handler(handle_client_message, NULL);
    
    // 5. 創建State Machine
    g_server_ctx = server_sm_create(config);
    if (g_server_ctx == NULL) {
        #ifndef TESTING
        logger_error("Failed to create state machine");
        #endif
        ws_server_cleanup();
        ps5_detector_cleanup();
        ps5_wake_cleanup();
        cec_monitor_cleanup();
        return -1;
    }
    
    server_sm_set_state_callback(g_server_ctx, on_state_enter, NULL);
    
    #ifndef TESTING
    logger_info("All modules initialized successfully");
    #endif
    
    return 0;
}

/**
 * @brief 清理所有模組
 */
static void cleanup_modules(void) {
    if (g_server_ctx) {
        server_sm_destroy(g_server_ctx);
        g_server_ctx = NULL;
    }
    
    ws_server_cleanup();
    ps5_detector_cleanup();
    ps5_wake_cleanup();
    cec_monitor_cleanup();
    
    #ifndef TESTING
    logger_info("All modules cleaned up");
    #endif
}

/**
 * @brief 主事件循環
 */
static void run_main_loop(void) {
    #ifndef TESTING
    logger_info("Entering main event loop");
    #endif
    
    // 啟動WebSocket Server
    ws_server_start();
    
    // 啟動CEC Monitor
    cec_monitor_start();
    
    while (g_running) {
        // 更新狀態機
        if (g_server_ctx) {
            server_sm_update(g_server_ctx);
        }
        
        // 服務WebSocket
        ws_server_service(100);  // 100ms timeout
        
        // 定期檢查PS5網路狀態
        static int check_counter = 0;
        if (++check_counter >= 100) {  // 每10秒檢查一次
            check_counter = 0;
            
            // 查詢PS5網路狀態
            ps5_info_t ps5_info = {0};
            int detect_result = ps5_detector_get_cached(&ps5_info);
            bool ps5_online = (detect_result == PS5_DETECT_OK && ps5_info.online);
            
            if (g_server_ctx) {
                // 注意: server_sm_on_ps5_network_changed 可能需要修改參數類型
                // 暫時使用 bool，如果需要可以轉換為其他類型
                // server_sm_on_ps5_network_changed(g_server_ctx, ps5_online);
                
                // 或者直接更新狀態，不調用這個函數
                // 因為 PS5 網路狀態變化會在 CEC 狀態變化時一起處理
            }
        }
        
        // 小延遲避免CPU過載
        usleep(100000);  // 100ms
    }
    
    // 停止服務
    ws_server_stop();
    cec_monitor_stop();
    
    #ifndef TESTING
    logger_info("Exiting main event loop");
    #endif
}

/* ============================================================
 *  Main Entry Point
 * ============================================================ */

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -d, --daemon        Run as daemon\n");
    printf("  -p, --port PORT     WebSocket server port (default: %d)\n", 
           DEFAULT_WS_PORT);
    printf("  -s, --subnet CIDR   PS5 subnet for detection (default: %s)\n", 
           DEFAULT_PS5_SUBNET);
    printf("  -c, --cache PATH    Cache file path (default: %s)\n", 
           DEFAULT_CACHE_PATH);
    printf("  -v, --version       Print version and exit\n");
    printf("  -h, --help          Print this help and exit\n");
    printf("\nExamples:\n");
    printf("  %s                  # Run in foreground\n", program_name);
    printf("  %s --daemon         # Run as daemon\n", program_name);
    printf("  %s -p 9090 -s 192.168.2.0/24\n", program_name);
}

static void print_version(void) {
    printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    server_config_t config = {0};
    config.ws_port = DEFAULT_WS_PORT;
    strncpy(config.ps5_subnet, DEFAULT_PS5_SUBNET, sizeof(config.ps5_subnet) - 1);
    strncpy(config.cache_path, DEFAULT_CACHE_PATH, sizeof(config.cache_path) - 1);
    
    // 解析命令列參數
    static struct option long_options[] = {
        {"daemon",  no_argument,       0, 'd'},
        {"port",    required_argument, 0, 'p'},
        {"subnet",  required_argument, 0, 's'},
        {"cache",   required_argument, 0, 'c'},
        {"version", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "dp:s:c:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = true;
                break;
            case 'p':
                config.ws_port = atoi(optarg);
                break;
            case 's':
                strncpy(config.ps5_subnet, optarg, sizeof(config.ps5_subnet) - 1);
                break;
            case 'c':
                strncpy(config.cache_path, optarg, sizeof(config.cache_path) - 1);
                break;
            case 'v':
                print_version();
                return 0;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // ⭐⭐⭐ STEP 1: 初始化 Platform (最重要!)
    #ifndef TESTING
    printf("Initializing gaming platform...\n");
    
    int result = platform_init();
    if (result != PLATFORM_OK) {
        fprintf(stderr, "ERROR: Failed to initialize platform: %d\n", result);
        return 1;
    }
    
    // ⭐⭐⭐ STEP 2: 驗證設備類型必須為 "server"
    const char *device_type = platform_get_device_type();
    if (strcmp(device_type, "server") != 0) {
        fprintf(stderr, "ERROR: This device is not a server!\n");
        fprintf(stderr, "Detected device type: %s\n", device_type);
        fprintf(stderr, "Expected device type: server\n");
        platform_cleanup();
        return 1;
    }
    
    const char *platform_version = platform_get_version();
    printf("Platform initialized successfully\n");
    printf("  Version: %s\n", platform_version);
    printf("  Device type: %s\n", device_type);
    #endif
    
    // Daemon模式
    if (daemon_mode) {
        if (daemon(0, 0) != 0) {
            perror("Failed to daemonize");
            #ifndef TESTING
            platform_cleanup();
            #endif
            return 1;
        }
    }
    
    // 初始化Logger
    #ifndef TESTING
    logger_init(PROGRAM_NAME, daemon_mode ? LOG_LEVEL_INFO : LOG_LEVEL_DEBUG,
                daemon_mode);
    logger_info("=== %s v%s starting ===", PROGRAM_NAME, PROGRAM_VERSION);
    logger_info("Platform: %s", platform_version);
    logger_info("Device type: %s", device_type);
    logger_info("WebSocket port: %d", config.ws_port);
    logger_info("PS5 subnet: %s", config.ps5_subnet);
    logger_info("Cache path: %s", config.cache_path);
    #endif
    
    // 設定信號處理
    setup_signal_handlers();
    
    // 初始化所有模組
    if (initialize_modules(&config) != 0) {
        #ifndef TESTING
        logger_error("Failed to initialize modules");
        logger_cleanup();
        platform_cleanup();
        #endif
        return 1;
    }
    
    // 執行主循環
    run_main_loop();
    
    // 清理
    cleanup_modules();
    
    #ifndef TESTING
    logger_info("=== %s shutdown complete ===", PROGRAM_NAME);
    logger_cleanup();
    
    // ⭐⭐⭐ STEP 3: 清理 Platform
    platform_cleanup();
    #endif
    
    return 0;
}
