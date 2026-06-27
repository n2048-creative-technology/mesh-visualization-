/**
 * Configuration file for ESP32 Mesh Network Node
 * Adaptive mesh topology with UDP unicast and dynamic TX power
 */

#pragma once

// ============================================================================
// WiFi and Mesh Settings
// ============================================================================

#define WIFI_CHANNEL          36      // 5 GHz channel (ESP32-C5)
#define MESH_MAX_HOPS          10
#define MESH_VOTE_PERCENT      1       // Self-healing percentage
#define MESH_ROUTER_SSID       "mesh_network"
#define MESH_ROUTER_PASS       "mesh_password"
#define MESH_ROUTER_BSSID      {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}

// ============================================================================
// UDP Settings
// ============================================================================

#define UDP_PORT              1234
#define UDP_MTU               1460    // Maximum UDP payload size

// Visualization subscriber configuration
#define VISUALIZATION_IP      "192.168.1.254"  // Laptop/RPi IP
#define VISUALIZATION_PORT    1234

// Static IP assignment: 192.168.1.<last_byte_of_mac>
#define IP_PREFIX             "192.168.1."

// ============================================================================
// Neighbor Settings
// ============================================================================

#define MAX_NEIGHBORS          8       // Maximum neighbors per node
#define RSSI_THRESHOLD         3       // dBm change to trigger neighbor list update
#define NEIGHBOR_TIMEOUT_MS    5000    // Remove neighbor if not seen for 5s
#define BEACON_SCAN_INTERVAL   100     // ms between beacon scans

// ============================================================================
// TX Power Settings
// ============================================================================

#define TARGET_RSSI           -65      // Target RSSI at weakest neighbor
#define MIN_TX_POWER          0       // dBm (8 = 0dBm, 20 = 8dBm for ESP32)
#define MAX_TX_POWER          20      // dBm (max for ESP32)
#define TX_POWER_STEP         2       // dBm adjustment step

// ============================================================================
// Node State Settings
// ============================================================================

#define STATE_UPDATE_INTERVAL 100     // ms (10 Hz)
#define TEMPERATURE_SCALE     10.0    // Multiply by 10 for int16_t storage

// ============================================================================
// Sensor Settings
// ============================================================================

#define MMWAVE_PRESENCE_PIN   GPIO_NUM_5
#define TEMPERATURE_PIN       GPIO_NUM_4

// ============================================================================
// Debug Settings
// ============================================================================

#define DEBUG_MESH            true
#define DEBUG_BEACON          true
#define DEBUG_UDP             true
#define DEBUG_NEIGHBORS       true
#define DEBUG_TX_POWER        true

// ============================================================================
// Message Types
// ============================================================================

#define MSG_TYPE_STATE_UPDATE 0x01
#define MSG_TYPE_BEACON       0x02
#define MSG_TYPE_FORWARD      0x03

// ============================================================================
// Protocol Version
// ============================================================================

#define PROTOCOL_VERSION      0x01
