/**
 * Beacon Monitor Header
 * Handles WiFi scanning and RSSI tracking for neighbor discovery
 * PlatformIO compatible for VS Code
 */

#pragma once

#include "config.h"
#include "esp32_arduino_compat.h"
#include <string.h>

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Beacon information structure
 */
typedef struct {
    uint8_t mac[6];          // BSSID (AP MAC address)
    int8_t rssi;            // Received signal strength
    uint8_t channel;         // WiFi channel
    uint8_t ssid[33];       // SSID (null-terminated)
    uint8_t ssid_len;       // SSID length
    uint8_t encryption_type; // Encryption type
} beacon_info_t;

// ============================================================================
// Function Declarations
// ============================================================================

// Initialization
void init_beacon_monitoring(void);
void start_beacon_scanning(void);
void start_promiscuous_mode(void);
void stop_promiscuous_mode(void);

// Beacon processing
void process_beacon_frame(uint8_t *frame, uint16_t len, int8_t rssi);
void process_scan_results(void);
bool extract_beacon_info(uint8_t *frame, uint16_t len, beacon_info_t *beacon);

// Utility functions
bool is_mesh_beacon(uint8_t *frame, uint16_t len);
bool is_valid_mac(uint8_t *mac);
void print_beacon_info(beacon_info_t *beacon);

// Periodic update
void update_beacon_monitoring(void);

// Callback functions
void wifi_promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type);

// ============================================================================
// Global Variables (extern)
// ============================================================================

extern bool beacon_monitoring_active;
extern uint32_t last_beacon_scan_time;
