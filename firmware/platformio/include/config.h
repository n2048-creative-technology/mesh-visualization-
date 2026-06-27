/**
 * Configuration file for ESP32 Mesh Network Node
 * Adaptive mesh topology with UDP unicast and dynamic TX power
 * Supports: ESP32-C3 (2.4 GHz), ESP32-C5 (5 GHz), Generic ESP32
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Node State Definitions
// ============================================================================

#define NODE_STATE_BOOTING 0
#define NODE_STATE_IDLE 1
#define NODE_STATE_ACTIVE 2
#define NODE_STATE_ERROR 3

// Node state structure (defined here so it's available everywhere)
typedef struct {
    uint8_t state;           // Node state (0=idle, 1=active, 2=error, 3=booting)
    uint8_t color[3];        // RGB color
    int16_t temperature;     // Temperature \u00d710
    uint8_t mmwave_presence; // 0 or 1
    uint32_t mmwave_distance; // Distance in mm
    uint32_t timestamp;      // Unix timestamp
} node_state_t;

// ============================================================================
// Platform Detection
// ============================================================================

// Define platform if not already defined
#ifndef ESP_PLATFORM

// Auto-detect platform based on ESP-IDF defines
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    #define ESP_PLATFORM ESP32_C3
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
    #define ESP_PLATFORM ESP32_C5
#elif defined(CONFIG_IDF_TARGET_ESP32)
    #define ESP_PLATFORM ESP32
#else
    #define ESP_PLATFORM ESP32_GENERIC
#endif

#endif // ESP_PLATFORM

// Platform-specific definitions
#define ESP32_C3  1
#define ESP32_C5  2
#define ESP32     3
#define ESP32_GENERIC 4

// ============================================================================
// WiFi and Mesh Settings
// ============================================================================

// Default channels (can be overridden in platformio.ini)
#ifndef WIFI_CHANNEL_24GHZ
#define WIFI_CHANNEL_24GHZ 6      // 2.4 GHz channel 6
#endif

#ifndef WIFI_CHANNEL_5GHZ
#define WIFI_CHANNEL_5GHZ 36     // 5 GHz channel 36
#endif

// Select channel based on platform
#if ESP_PLATFORM == ESP32_C3 || ESP_PLATFORM == ESP32 || ESP_PLATFORM == ESP32_GENERIC
    #ifndef WIFI_CHANNEL
    #define WIFI_CHANNEL WIFI_CHANNEL_24GHZ  // Default to 2.4 GHz for C3 and generic ESP32
    #endif
#elif ESP_PLATFORM == ESP32_C5
    #ifndef WIFI_CHANNEL
    #define WIFI_CHANNEL WIFI_CHANNEL_5GHZ   // Default to 5 GHz for C5
    #endif
#else
    #define WIFI_CHANNEL WIFI_CHANNEL_24GHZ  // Fallback to 2.4 GHz
#endif

// Mesh configuration
#ifndef MESH_MAX_HOPS
#define MESH_MAX_HOPS 10
#endif

#ifndef MESH_VOTE_PERCENT
#define MESH_VOTE_PERCENT 1       // Self-healing percentage
#endif

// Mesh network credentials (configure in platformio.ini or menuconfig)
#define MESH_ROUTER_SSID "mesh_network"
#define MESH_ROUTER_PASS "mesh_password"

// Mesh router BSSID (optional - set to null to auto-select)
// #define MESH_ROUTER_BSSID {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}

// ============================================================================
// UDP Settings
// ============================================================================

#ifndef UDP_PORT
#define UDP_PORT 1234
#endif

#ifndef UDP_MTU
#define UDP_MTU 1460    // Maximum UDP payload size
#endif

// Visualization subscriber configuration
#ifndef VISUALIZATION_IP
#define VISUALIZATION_IP "192.168.1.254"  // Laptop/RPi IP
#endif

#ifndef VISUALIZATION_PORT
#define VISUALIZATION_PORT 1234
#endif

// Static IP assignment: 192.168.1.<last_byte_of_mac>
#define IP_PREFIX "192.168.1."

// ============================================================================
// Neighbor Settings
// ============================================================================

#ifndef MAX_NEIGHBORS
#define MAX_NEIGHBORS 8       // Maximum neighbors per node
#endif

#ifndef RSSI_THRESHOLD
#define RSSI_THRESHOLD 3       // dBm change to trigger neighbor list update
#endif

#ifndef NEIGHBOR_TIMEOUT_MS
#define NEIGHBOR_TIMEOUT_MS 5000    // Remove neighbor if not seen for 5s
#endif

#ifndef BEACON_SCAN_INTERVAL
#define BEACON_SCAN_INTERVAL 100     // ms between beacon scans
#endif

// ============================================================================
// TX Power Settings
// ============================================================================

#ifndef TARGET_RSSI
#define TARGET_RSSI -65      // Target RSSI at weakest neighbor
#endif

#ifndef MIN_TX_POWER
#define MIN_TX_POWER 0       // dBm (8 = 0dBm, 20 = 8dBm for ESP32)
#endif

#ifndef MAX_TX_POWER
#define MAX_TX_POWER 20      // dBm (max for ESP32)
#endif

#ifndef TX_POWER_STEP
#define TX_POWER_STEP 2       // dBm adjustment step
#endif

// ============================================================================
// Node State Settings
// ============================================================================

#ifndef STATE_UPDATE_INTERVAL
#define STATE_UPDATE_INTERVAL 100     // ms (10 Hz)
#endif

#ifndef TEMPERATURE_SCALE
#define TEMPERATURE_SCALE 10.0    // Multiply by 10 for int16_t storage
#endif

// ============================================================================
// Sensor Settings
// ============================================================================

// LED pins - can be customized per platform
#ifndef LED_RED_PIN
#define LED_RED_PIN GPIO_NUM_1
#endif

#ifndef LED_GREEN_PIN
#define LED_GREEN_PIN GPIO_NUM_2
#endif

#ifndef LED_BLUE_PIN
#define LED_BLUE_PIN GPIO_NUM_3
#endif

// Sensor pins
#ifndef MMWAVE_PRESENCE_PIN
#define MMWAVE_PRESENCE_PIN GPIO_NUM_5
#endif

#ifndef TEMPERATURE_PIN
#define TEMPERATURE_PIN GPIO_NUM_4
#endif

// ============================================================================
// Debug Settings
// ============================================================================

// Debug flags (can be enabled in platformio.ini)
#ifndef DEBUG_MESH
#define DEBUG_MESH false
#endif

#ifndef DEBUG_BEACON
#define DEBUG_BEACON false
#endif

#ifndef DEBUG_UDP
#define DEBUG_UDP false
#endif

#ifndef DEBUG_NEIGHBORS
#define DEBUG_NEIGHBORS false
#endif

#ifndef DEBUG_TX_POWER
#define DEBUG_TX_POWER false
#endif

// ============================================================================
// Message Types
// ============================================================================

#define MSG_TYPE_STATE_UPDATE 0x01
#define MSG_TYPE_BEACON 0x02
#define MSG_TYPE_FORWARD 0x03

// ============================================================================
// Protocol Version
// ============================================================================

#define PROTOCOL_VERSION 0x01

// ============================================================================
// Platform-Specific Configuration
// ============================================================================

// ESP32-C3 specific settings
#if ESP_PLATFORM == ESP32_C3
    #define PLATFORM_NAME "ESP32-C3"
    #define SUPPORTS_5GHZ false
    #define SUPPORTS_24GHZ true
    
// ESP32-C5 specific settings
#elif ESP_PLATFORM == ESP32_C5
    #define PLATFORM_NAME "ESP32-C5"
    #define SUPPORTS_5GHZ true
    #define SUPPORTS_24GHZ true
    
// Generic ESP32 specific settings
#elif ESP_PLATFORM == ESP32 || ESP_PLATFORM == ESP32_GENERIC
    #define PLATFORM_NAME "ESP32"
    #define SUPPORTS_5GHZ false
    #define SUPPORTS_24GHZ true
    
#else
    #define PLATFORM_NAME "Unknown"
    #define SUPPORTS_5GHZ false
    #define SUPPORTS_24GHZ true
#endif

// ============================================================================
// Utility Macros
// ============================================================================

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
