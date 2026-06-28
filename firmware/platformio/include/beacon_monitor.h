/**
 * Beacon Monitor Header - ESP-IDF Version
 */

#pragma once

#include <string.h>
#include "esp_wifi.h"

// Data Structures
typedef struct {
    uint8_t mac[6];
    int8_t rssi;
    uint8_t channel;
    uint8_t ssid[33];
    uint8_t ssid_len;
    uint8_t encryption_type;
} beacon_info_t;

// Function Declarations
void init_beacon_monitoring(void);
void start_beacon_scanning(void);
void start_promiscuous_mode(void);
void stop_promiscuous_mode(void);
void process_beacon_frame(uint8_t *frame, uint16_t len, int8_t rssi);
void process_scan_results(void);
bool extract_beacon_info(uint8_t *frame, uint16_t len, beacon_info_t *beacon);
bool is_mesh_beacon(uint8_t *frame, uint16_t len);
bool is_valid_mac(uint8_t *mac);
void print_beacon_info(beacon_info_t *beacon);
void update_beacon_monitoring(void);
void wifi_promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type);

// Global Variables
extern bool beacon_monitoring_active;
extern uint32_t last_beacon_scan_time;
