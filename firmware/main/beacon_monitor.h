/**
 * Beacon Monitor Header
 * Handles promiscuous mode scanning and RSSI tracking
 */

#pragma once

#include "config.h"
#include "mesh_node.h"
#include <esp_wifi.h>

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Beacon frame information
 */
typedef struct {
    uint8_t mac[6];      // BSSID (AP MAC)
    int8_t rssi;         // Received signal strength
    uint8_t channel;      // Channel
    uint16_t beacon_interval; // Beacon interval in TU (1.024ms)
    uint8_t ssid[33];     // SSID (null-terminated)
    uint8_t ssid_len;     // SSID length
} beacon_info_t;

// ============================================================================
// Function Declarations
// ============================================================================

// Initialization
void init_beacon_monitoring(void);
void start_promiscuous_mode(void);
void stop_promiscuous_mode(void);

// Beacon processing
void process_beacon_frame(uint8_t *frame, uint16_t len, int8_t rssi);
void extract_beacon_info(uint8_t *frame, uint16_t len, beacon_info_t *beacon);

// Callbacks
void wifi_promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type);

// Utility functions
bool is_mesh_beacon(uint8_t *frame, uint16_t len);
bool is_valid_mac(uint8_t *mac);
void print_beacon_info(beacon_info_t *beacon);

// ============================================================================
// Global Variables (extern)
// ============================================================================

extern bool beacon_monitoring_active;
extern uint32_t last_beacon_scan_time;
