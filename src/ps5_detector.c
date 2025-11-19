/**
 * @file ps5_detector.c
 * @brief PS5 Detector Implementation - Network detection for PS5
 * 
 * @author Gaming System Development Team
 * @date 2025-11-05
 * @version 1.0.1 (Fixed IP validation and string copy issues)
 */

// ⭐ POSIX 標準定義 (必須在最前面!)
#define _POSIX_C_SOURCE 200809L

#include "ps5_detector.h"

// Standard C library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

// POSIX headers
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// cJSON for cache management
#include <cjson/cJSON.h>

/* ============================================================
 *  Constants
 * ============================================================ */

#define PS5_DEFAULT_PORT    9295        // PS5 Remote Play port
#define COMMAND_BUFFER_SIZE 1024
#define OUTPUT_BUFFER_SIZE  4096
#define PING_TIMEOUT_SEC    2

/* ============================================================
 *  Internal Structures
 * ============================================================ */

typedef struct {
    char subnet[PS5_SUBNET_MAX_LEN];
    char cache_path[256];
    bool initialized;
    ps5_info_t cached_info;
    time_t cache_timestamp;
} ps5_detector_context_t;

/* ============================================================
 *  Static Variables
 * ============================================================ */

static ps5_detector_context_t g_detector_ctx = {0};

/* ============================================================
 *  Helper Functions - Command Execution
 * ============================================================ */

/**
 * @brief Execute shell command and get output
 */
static int execute_command(const char *cmd, char *output, size_t output_size) {
    if (cmd == NULL || output == NULL || output_size == 0) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        #ifndef TESTING
        fprintf(stderr, "[PS5Detect] Failed to execute: %s\n", cmd);
        #endif
        return PS5_DETECT_ERROR_SCAN_FAILED;
    }
    
    // Read output
    memset(output, 0, output_size);
    size_t bytes_read = 0;
    
    while (fgets(output + bytes_read, output_size - bytes_read, fp) != NULL) {
        bytes_read = strlen(output);
        if (bytes_read >= output_size - 1) {
            break;
        }
    }
    
    int status = pclose(fp);
    if (status != 0 && bytes_read == 0) {
        return PS5_DETECT_ERROR_SCAN_FAILED;
    }
    
    return PS5_DETECT_OK;
}

/* ============================================================
 *  Helper Functions - String Validation
 * ============================================================ */

bool ps5_detector_validate_mac(const char *mac) {
    if (mac == NULL) {
        return false;
    }
    
    // MAC format: XX:XX:XX:XX:XX:XX (17 chars)
    if (strlen(mac) != 17) {
        return false;
    }
    
    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            // Should be ':'
            if (mac[i] != ':') {
                return false;
            }
        } else {
            // Should be hex digit
            if (!isxdigit(mac[i])) {
                return false;
            }
        }
    }
    
    return true;
}

bool ps5_detector_validate_ip(const char *ip) {
    if (ip == NULL || strlen(ip) == 0) {
        return false;
    }
    
    // ✅ FIX: Enhanced IP validation with range checking
    int octet_count = 0;
    int current_octet = 0;
    int digits = 0;
    bool has_digit = false;
    
    for (const char *p = ip; *p != '\0'; p++) {
        if (*p == '.') {
            // Check if we have valid digits before the dot
            if (!has_digit || digits == 0 || digits > 3) {
                return false;
            }
            
            // Validate octet count and range (0-255)
            if (octet_count >= 3) {
                return false;  // Already have 3 octets, can't have more dots
            }
            if (current_octet > 255) {
                return false;  // Octet out of range
            }
            
            octet_count++;
            
            // Reset for next octet
            current_octet = 0;
            digits = 0;
            has_digit = false;
            
        } else if (isdigit(*p)) {
            // Build the current octet value
            current_octet = current_octet * 10 + (*p - '0');
            digits++;
            has_digit = true;
            
            // Early check to avoid overflow
            if (current_octet > 255) {
                return false;
            }
        } else {
            // Invalid character
            return false;
        }
    }
    
    // Validate the last octet
    if (!has_digit || digits == 0 || digits > 3 || current_octet > 255) {
        return false;
    }
    
    // Must have exactly 3 dots (which means 4 octets)
    return (octet_count == 3);
}

/* ============================================================
 *  Helper Functions - Cache Management
 * ============================================================ */

/**
 * @brief Load cache from JSON file
 */
static int load_cache_from_file(ps5_info_t *info) {
    if (info == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    // ✅ FIX: Initialize the structure to zero first
    memset(info, 0, sizeof(ps5_info_t));
    
    // Check if cache file exists
    if (access(g_detector_ctx.cache_path, F_OK) != 0) {
        return PS5_DETECT_ERROR_CACHE_INVALID;
    }
    
    // Read cache file
    FILE *fp = fopen(g_detector_ctx.cache_path, "r");
    if (fp == NULL) {
        return PS5_DETECT_ERROR_CACHE_INVALID;
    }
    
    // Read file content
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 4096) {
        fclose(fp);
        return PS5_DETECT_ERROR_CACHE_INVALID;
    }
    
    char *json_str = (char*)malloc(file_size + 1);
    if (json_str == NULL) {
        fclose(fp);
        return PS5_DETECT_ERROR_UNKNOWN;
    }
    
    size_t bytes_read = fread(json_str, 1, file_size, fp);
    json_str[bytes_read] = '\0';
    fclose(fp);
    
    // Parse JSON
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    
    if (root == NULL) {
        return PS5_DETECT_ERROR_CACHE_INVALID;
    }
    
    // Extract fields
    cJSON *ip_item = cJSON_GetObjectItem(root, "ip");
    cJSON *mac_item = cJSON_GetObjectItem(root, "mac");
    cJSON *last_seen_item = cJSON_GetObjectItem(root, "last_seen");
    cJSON *online_item = cJSON_GetObjectItem(root, "online");
    
    if (ip_item == NULL || !cJSON_IsString(ip_item) ||
        mac_item == NULL || !cJSON_IsString(mac_item) ||
        last_seen_item == NULL || !cJSON_IsNumber(last_seen_item)) {
        cJSON_Delete(root);
        return PS5_DETECT_ERROR_CACHE_INVALID;
    }
    
    // ✅ FIX: Use snprintf for safe string copying with guaranteed null terminator
    snprintf(info->ip, PS5_IP_MAX_LEN, "%s", ip_item->valuestring);
    snprintf(info->mac, PS5_MAC_MAX_LEN, "%s", mac_item->valuestring);
    info->last_seen = (time_t)last_seen_item->valuedouble;
    info->online = (online_item != NULL && cJSON_IsTrue(online_item));
    
    cJSON_Delete(root);
    
    // Validate cache age
    time_t now = time(NULL);
    if (now - info->last_seen > PS5_CACHE_MAX_AGE) {
        return PS5_DETECT_ERROR_CACHE_INVALID;
    }
    
    return PS5_DETECT_OK;
}

/**
 * @brief Save cache to JSON file
 */
static int save_cache_to_file(const ps5_info_t *info) {
    if (info == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    // Create JSON object
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return PS5_DETECT_ERROR_UNKNOWN;
    }
    
    cJSON_AddStringToObject(root, "ip", info->ip);
    cJSON_AddStringToObject(root, "mac", info->mac);
    cJSON_AddNumberToObject(root, "last_seen", (double)info->last_seen);
    cJSON_AddBoolToObject(root, "online", info->online);
    
    // Convert to string
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        return PS5_DETECT_ERROR_UNKNOWN;
    }
    
    // Write to file
    FILE *fp = fopen(g_detector_ctx.cache_path, "w");
    if (fp == NULL) {
        cJSON_free(json_str);
        return PS5_DETECT_ERROR_CACHE_INVALID;
    }
    
    fputs(json_str, fp);
    fclose(fp);
    cJSON_free(json_str);
    
    return PS5_DETECT_OK;
}

/* ============================================================
 *  Helper Functions - Detection Methods
 * ============================================================ */

/**
 * @brief Check ARP table for PS5
 */
static int check_arp_table(ps5_info_t *info) {
    if (info == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    char cmd[COMMAND_BUFFER_SIZE];
    char output[OUTPUT_BUFFER_SIZE];
    
    snprintf(cmd, sizeof(cmd), "arp -n");
    
    if (execute_command(cmd, output, sizeof(output)) != PS5_DETECT_OK) {
        return PS5_DETECT_ERROR_NOT_FOUND;
    }
    
    // Parse ARP output
    // Format: IP address       HW type     Flags       HW address            Mask     Device
    // 192.168.1.100    0x1         0x2         aa:bb:cc:dd:ee:ff     *        eth0
    
    char *line = strtok(output, "\n");
    while (line != NULL) {
        char ip[PS5_IP_MAX_LEN];
        char mac[PS5_MAC_MAX_LEN];
        
        // Try to parse line
        if (sscanf(line, "%15s %*s %*s %17s", ip, mac) == 2) {
            if (ps5_detector_validate_ip(ip) && ps5_detector_validate_mac(mac)) {
                // Found a valid entry
                // TODO: Add PS5 MAC address verification here
                // For now, accept any valid entry in our subnet
                snprintf(info->ip, PS5_IP_MAX_LEN, "%s", ip);
                snprintf(info->mac, PS5_MAC_MAX_LEN, "%s", mac);
                info->last_seen = time(NULL);
                info->online = true;
                return PS5_DETECT_OK;
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    return PS5_DETECT_ERROR_NOT_FOUND;
}

/**
 * @brief Scan network using nmap
 */
static int scan_network_nmap(ps5_info_t *info) {
    if (info == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    char cmd[COMMAND_BUFFER_SIZE];
    char output[OUTPUT_BUFFER_SIZE];
    
    // Use nmap to scan for PS5 Remote Play port (9295)
    snprintf(cmd, sizeof(cmd), 
             "nmap -p %d --open %s 2>/dev/null | grep -B 4 'open'",
             PS5_DEFAULT_PORT, g_detector_ctx.subnet);
    
    #ifdef TESTING
    // In test mode, simulate not found
    return PS5_DETECT_ERROR_NOT_FOUND;
    #endif
    
    if (execute_command(cmd, output, sizeof(output)) != PS5_DETECT_OK) {
        return PS5_DETECT_ERROR_NOT_FOUND;
    }
    
    // Parse nmap output to find IP
    // Look for "Nmap scan report for X.X.X.X"
    char *line = strtok(output, "\n");
    while (line != NULL) {
        if (strstr(line, "Nmap scan report for") != NULL) {
            // Extract IP address
            char *ip_start = strrchr(line, ' ');
            if (ip_start != NULL) {
                ip_start++; // Skip space
                if (ps5_detector_validate_ip(ip_start)) {
                    snprintf(info->ip, PS5_IP_MAX_LEN, "%s", ip_start);
                    info->last_seen = time(NULL);
                    info->online = true;
                    // MAC will be empty, need ARP lookup
                    info->mac[0] = '\0';
                    return PS5_DETECT_OK;
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    
    return PS5_DETECT_ERROR_NOT_FOUND;
}

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

int ps5_detector_init(const char *subnet, const char *cache_path) {
    if (g_detector_ctx.initialized) {
        return PS5_DETECT_ERROR_NOT_INIT;
    }
    
    if (subnet == NULL || cache_path == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    // Copy configuration
    strncpy(g_detector_ctx.subnet, subnet, PS5_SUBNET_MAX_LEN - 1);
    g_detector_ctx.subnet[PS5_SUBNET_MAX_LEN - 1] = '\0';
    strncpy(g_detector_ctx.cache_path, cache_path, sizeof(g_detector_ctx.cache_path) - 1);
    g_detector_ctx.cache_path[sizeof(g_detector_ctx.cache_path) - 1] = '\0';
    
    // Initialize state
    memset(&g_detector_ctx.cached_info, 0, sizeof(ps5_info_t));
    g_detector_ctx.cache_timestamp = 0;
    g_detector_ctx.initialized = true;
    
    #ifndef TESTING
    fprintf(stdout, "[PS5Detect] Initialized: subnet=%s, cache=%s\n", 
            subnet, cache_path);
    #endif
    
    return PS5_DETECT_OK;
}

int ps5_detector_get_cached(ps5_info_t *info) {
    if (!g_detector_ctx.initialized) {
        return PS5_DETECT_ERROR_NOT_INIT;
    }
    
    if (info == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    return load_cache_from_file(info);
}

int ps5_detector_save_cache(const ps5_info_t *info) {
    if (!g_detector_ctx.initialized) {
        return PS5_DETECT_ERROR_NOT_INIT;
    }
    
    if (info == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    // Update internal cache
    memcpy(&g_detector_ctx.cached_info, info, sizeof(ps5_info_t));
    g_detector_ctx.cache_timestamp = time(NULL);
    
    return save_cache_to_file(info);
}

bool ps5_detector_ping(const char *ip) {
    if (ip == NULL || !ps5_detector_validate_ip(ip)) {
        return false;
    }
    
    char cmd[COMMAND_BUFFER_SIZE];
    char output[OUTPUT_BUFFER_SIZE];
    
    // Ping with 1 packet, 2 second timeout
    snprintf(cmd, sizeof(cmd), 
             "ping -c 1 -W %d %s 2>/dev/null",
             PING_TIMEOUT_SEC, ip);
    
    #ifdef TESTING
    // In test mode, simulate success for valid IPs
    return true;
    #endif
    
    int result = execute_command(cmd, output, sizeof(output));
    
    // Check if ping was successful
    return (result == PS5_DETECT_OK && strstr(output, "1 received") != NULL);
}

int ps5_detector_scan(ps5_info_t *info) {
    if (!g_detector_ctx.initialized) {
        return PS5_DETECT_ERROR_NOT_INIT;
    }
    
    if (info == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    #ifndef TESTING
    fprintf(stdout, "[PS5Detect] Starting full network scan...\n");
    #endif
    
    // Try nmap scan
    int result = scan_network_nmap(info);
    
    if (result == PS5_DETECT_OK) {
        // If we found IP, try to get MAC from ARP
        if (info->mac[0] == '\0') {
            ps5_info_t arp_info;
            if (check_arp_table(&arp_info) == PS5_DETECT_OK) {
                snprintf(info->mac, PS5_MAC_MAX_LEN, "%s", arp_info.mac);
            }
        }
        
        // Save to cache
        ps5_detector_save_cache(info);
    }
    
    return result;
}

int ps5_detector_quick_check(const char *cached_ip, ps5_info_t *info) {
    if (!g_detector_ctx.initialized) {
        return PS5_DETECT_ERROR_NOT_INIT;
    }
    
    if (info == NULL) {
        return PS5_DETECT_ERROR_INVALID_PARAM;
    }
    
    // Step 1: Try cache
    if (ps5_detector_get_cached(info) == PS5_DETECT_OK) {
        // Verify with ping
        if (ps5_detector_ping(info->ip)) {
            info->online = true;
            info->last_seen = time(NULL);
            return PS5_DETECT_OK;
        }
    }
    
    // Step 2: Try ARP table
    if (check_arp_table(info) == PS5_DETECT_OK) {
        ps5_detector_save_cache(info);
        return PS5_DETECT_OK;
    }
    
    // Step 3: Full scan (slow)
    return ps5_detector_scan(info);
}

int ps5_detector_clear_cache(void) {
    if (!g_detector_ctx.initialized) {
        return PS5_DETECT_ERROR_NOT_INIT;
    }
    
    if (unlink(g_detector_ctx.cache_path) != 0 && errno != ENOENT) {
        return PS5_DETECT_ERROR_CACHE_INVALID;
    }
    
    memset(&g_detector_ctx.cached_info, 0, sizeof(ps5_info_t));
    g_detector_ctx.cache_timestamp = 0;
    
    return PS5_DETECT_OK;
}

time_t ps5_detector_get_cache_age(void) {
    struct stat st;
    if (stat(g_detector_ctx.cache_path, &st) != 0) {
        return -1;
    }
    
    time_t now = time(NULL);
    return now - st.st_mtime;
}

void ps5_detector_cleanup(void) {
    if (!g_detector_ctx.initialized) {
        return;
    }
    
    memset(&g_detector_ctx, 0, sizeof(ps5_detector_context_t));
    
    #ifndef TESTING
    fprintf(stdout, "[PS5Detect] Cleaned up\n");
    #endif
}

/* ============================================================
 *  String Conversion Functions
 * ============================================================ */

const char* ps5_detector_error_string(int error) {
    switch (error) {
        case PS5_DETECT_OK:                     return "OK";
        case PS5_DETECT_ERROR_NOT_INIT:         return "Not initialized";
        case PS5_DETECT_ERROR_NOT_FOUND:        return "PS5 not found";
        case PS5_DETECT_ERROR_INVALID_PARAM:    return "Invalid parameter";
        case PS5_DETECT_ERROR_CACHE_INVALID:    return "Cache invalid";
        case PS5_DETECT_ERROR_SCAN_FAILED:      return "Scan failed";
        case PS5_DETECT_ERROR_UNKNOWN:          return "Unknown error";
        default:                                return "Invalid error code";
    }
}

const char* ps5_detector_method_string(detect_method_t method) {
    switch (method) {
        case DETECT_METHOD_CACHE:   return "CACHE";
        case DETECT_METHOD_ARP:     return "ARP";
        case DETECT_METHOD_SCAN:    return "SCAN";
        case DETECT_METHOD_PING:    return "PING";
        default:                    return "UNKNOWN";
    }
}
