/**
 * @file ps5_detector.h
 * @brief PS5 Detector - Network detection for PS5 console
 * 
 * This module detects PS5 on the network using multiple methods:
 * 1. Cache lookup (fastest, <1ms)
 * 2. ARP table query (fast, ~10ms)
 * 3. Network scan with nmap (slow, ~5-30s)
 * 
 * @author Gaming System Development Team
 * @date 2025-11-05
 * @version 1.0.0
 */

#ifndef PS5_DETECTOR_H
#define PS5_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  Error Codes
 * ============================================================ */

#define PS5_DETECT_OK                   0
#define PS5_DETECT_ERROR_NOT_INIT      -1
#define PS5_DETECT_ERROR_NOT_FOUND     -2
#define PS5_DETECT_ERROR_INVALID_PARAM -3
#define PS5_DETECT_ERROR_CACHE_INVALID -4
#define PS5_DETECT_ERROR_SCAN_FAILED   -5
#define PS5_DETECT_ERROR_UNKNOWN       -99

/* ============================================================
 *  Constants
 * ============================================================ */

#define PS5_IP_MAX_LEN      16    /**< Max IP address length */
#define PS5_MAC_MAX_LEN     18    /**< Max MAC address length */
#define PS5_SUBNET_MAX_LEN  32    /**< Max subnet string length */
#define PS5_CACHE_MAX_AGE   3600  /**< Cache valid for 1 hour (seconds) */

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief PS5 information structure
 */
typedef struct {
    char ip[PS5_IP_MAX_LEN];        /**< IP address */
    char mac[PS5_MAC_MAX_LEN];      /**< MAC address */
    time_t last_seen;                /**< Last seen timestamp */
    bool online;                     /**< Online status */
} ps5_info_t;

/**
 * @brief Detection method
 */
typedef enum {
    DETECT_METHOD_CACHE = 0,    /**< Cache lookup */
    DETECT_METHOD_ARP,          /**< ARP table query */
    DETECT_METHOD_SCAN,         /**< Network scan (nmap) */
    DETECT_METHOD_PING,         /**< Ping check */
} detect_method_t;

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Initialize PS5 detector
 * 
 * @param subnet Network subnet (e.g., "192.168.1.0/24")
 * @param cache_path Path to cache file (e.g., "/var/run/gaming/ps5_cache.json")
 * @return PS5_DETECT_OK on success, negative error code on failure
 */
int ps5_detector_init(const char *subnet, const char *cache_path);

/**
 * @brief Perform full network scan (slow, comprehensive)
 * 
 * This function performs a complete network scan using nmap.
 * It may take 5-30 seconds depending on network size.
 * 
 * @param info Pointer to store PS5 information
 * @return PS5_DETECT_OK if found, negative error code if not found
 */
int ps5_detector_scan(ps5_info_t *info);

/**
 * @brief Quick check using cache and ARP (fast)
 * 
 * This function first checks cache, then ARP table.
 * Falls back to scan only if needed.
 * 
 * @param cached_ip Cached IP address (can be NULL)
 * @param info Pointer to store PS5 information
 * @return PS5_DETECT_OK if found, negative error code if not found
 */
int ps5_detector_quick_check(const char *cached_ip, ps5_info_t *info);

/**
 * @brief Get cached PS5 information
 * 
 * @param info Pointer to store PS5 information
 * @return PS5_DETECT_OK on success, negative error code if cache invalid
 */
int ps5_detector_get_cached(ps5_info_t *info);

/**
 * @brief Save PS5 information to cache
 * 
 * @param info PS5 information to save
 * @return PS5_DETECT_OK on success, negative error code on failure
 */
int ps5_detector_save_cache(const ps5_info_t *info);

/**
 * @brief Ping check if PS5 is online
 * 
 * @param ip IP address to ping
 * @return true if online, false if offline
 */
bool ps5_detector_ping(const char *ip);

/**
 * @brief Validate MAC address format
 * 
 * @param mac MAC address string
 * @return true if valid format, false otherwise
 */
bool ps5_detector_validate_mac(const char *mac);

/**
 * @brief Validate IP address format
 * 
 * @param ip IP address string
 * @return true if valid format, false otherwise
 */
bool ps5_detector_validate_ip(const char *ip);

/**
 * @brief Clear cache file
 * 
 * @return PS5_DETECT_OK on success, negative error code on failure
 */
int ps5_detector_clear_cache(void);

/**
 * @brief Get cache age in seconds
 * 
 * @return Cache age in seconds, -1 if cache doesn't exist
 */
time_t ps5_detector_get_cache_age(void);

/**
 * @brief Clean up detector resources
 */
void ps5_detector_cleanup(void);

/**
 * @brief Convert error code to string
 * 
 * @param error Error code
 * @return Error message string
 */
const char* ps5_detector_error_string(int error);

/**
 * @brief Convert detection method to string
 * 
 * @param method Detection method
 * @return Method name string
 */
const char* ps5_detector_method_string(detect_method_t method);

#ifdef __cplusplus
}
#endif

#endif /* PS5_DETECTOR_H */
