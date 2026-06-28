/**
 * Configuration file for ESP32-C3 WiFi Mesh Network Node
 * ESP-IDF Framework Implementation
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

// Node state structure
typedef struct {
    uint8_t state;           // Node state (0=booting, 1=idle, 2=active, 3=error)
    uint8_t color[3];        // RGB color
    int16_t temperature;     // Temperature ×10
    uint8_t mmwave_presence; // 0 or 1
    uint32_t mmwave_distance; // Distance in mm
    uint32_t timestamp;      // Unix timestamp
} node_state_t;

// ============================================================================
// Platform Configuration - Seeed XIAO ESP32-C3 Only
// ============================================================================

#define PLATFORM_NAME "Seeed XIAO ESP32-C3"

// ============================================================================
// WiFi and Mesh Settings
// ============================================================================

// ESP32-C3 uses 2.4 GHz only
#define WIFI_CHANNEL 6

// Mesh configuration
#define MESH_MAX_HOPS 10
#define MESH_VOTE_PERCENT 1

// Mesh network credentials
#define MESH_ROUTER_SSID "n2048"
#define MESH_ROUTER_PASS "16377240"

// ============================================================================
// UDP Settings
// ============================================================================

#define UDP_PORT 1234

// Visualization server configuration
#define VISUALIZATION_IP "10.65.5.196"
#define VISUALIZATION_PORT 1234

// ============================================================================
// MQTT Configuration
// ============================================================================

#define ENABLE_MQTT_VISUALIZATION 1
#define MQTT_BROKER_IP "10.65.5.196"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "esp32_mesh_node"
#define MQTT_TOPOLOGY_TOPIC "mesh/topology"
#define MQTT_STATE_TOPIC "mesh/state"
#define MQTT_UPDATE_INTERVAL_MS 5000

// ============================================================================
// TX Power Settings
// ============================================================================

#define TARGET_RSSI -65
#define MIN_TX_POWER 0
#define MAX_TX_POWER 20
#define TX_POWER_STEP 2

// ============================================================================
// IP Configuration
// ============================================================================

#define IP_PREFIX "10.65."
#define IP_SUBNET_MASK "255.240.0.0"

// ============================================================================
// Neighbor Settings
// ============================================================================

#define MAX_NEIGHBORS 50
#define RSSI_THRESHOLD 3
#define NEIGHBOR_TIMEOUT_MS 5000
#define BEACON_SCAN_INTERVAL 100

// ============================================================================
// State Management Settings
// ============================================================================

#define STATE_UPDATE_INTERVAL 100
#define TEMPERATURE_SCALE 10.0

// ============================================================================
// LED Configuration
// ============================================================================

#define NUM_LEDS 1
#define DATA_PIN 10
#define TRANSITION_SPEED 0.002
#define TEMP_READ_INTERVAL_MS 5000

// ============================================================================
// Sensor and Hardware Configuration
// ESP32-C3 GPIO definitions
// ============================================================================

// LED PWM pins (adjust based on your hardware)
#define LED_RED_PIN 1
#define LED_GREEN_PIN 2
#define LED_BLUE_PIN 3

// Sensor pins
#define MMWAVE_PRESENCE_PIN 5
#define TEMPERATURE_PIN 4

// ============================================================================
// Debug Settings
// ============================================================================

#define DEBUG_BEACON false
#define DEBUG_UDP false
#define DEBUG_NEIGHBORS false
#define DEBUG_TX_POWER false

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
// Utility Macros
// ============================================================================

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

// ============================================================================
// ESP-IDF Compatibility
// ============================================================================

// For ESP-IDF, GPIO_NUM_* macros are not needed - use raw numbers
#define GPIO_NUM_0 0
#define GPIO_NUM_1 1
#define GPIO_NUM_2 2
#define GPIO_NUM_3 3
#define GPIO_NUM_4 4
#define GPIO_NUM_5 5
#define GPIO_NUM_6 6
#define GPIO_NUM_7 7
#define GPIO_NUM_8 8
#define GPIO_NUM_9 9
#define GPIO_NUM_10 10
#define GPIO_NUM_11 11
#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_15 15
#define GPIO_NUM_16 16
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_19 19
