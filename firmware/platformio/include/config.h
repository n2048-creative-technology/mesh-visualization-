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

// Emergent cellular automata state, based on n2048-creative-technology/emergent-esp32.
#define KERNEL_SIZE 9
#define MAX_ACTIVATIONS 8

typedef struct {
    uint8_t op;     // 0="<", 1="<=", 2="==", 3=">=", 4=">"
    float value;
} activation_rule_t;

// Node state structure
typedef struct {
    uint8_t state;           // Node state (0=booting, 1=idle, 2=active, 3=error)
    uint8_t color[3];        // RGB color
    int16_t temperature;     // Temperature ×10
    uint8_t mmwave_presence; // 0 or 1
    uint32_t mmwave_distance; // Distance in mm
    uint32_t timestamp;      // Unix timestamp
    uint8_t value;           // Binary CA value used for LED output
    uint32_t kernel_sequence;
    uint32_t value_sequence;
    uint32_t activation_sequence;
    float kernel[KERNEL_SIZE]; // n0..n7 + self
    activation_rule_t activations[MAX_ACTIVATIONS];
    uint8_t activation_count;
    float activation_sum;
} node_state_t;

// ============================================================================
// Platform Configuration
// ============================================================================

#define PLATFORM_NAME "Seeed XIAO ESP32-C3"

// ============================================================================
// WiFi and Mesh Settings
// ============================================================================

// ESP32-C3 uses 2.4 GHz only. Channel 0 lets ESP-WiFi-Mesh scan for the router.
#ifndef NETWORK_ENV_HOME
#define WIFI_CHANNEL 6
#else
#define WIFI_CHANNEL 0
#endif

// Mesh configuration
#define MESH_TARGET_NODE_COUNT 1000
#define MESH_MAX_HOPS 10
#define MESH_VOTE_PERCENT 1
#define MESH_AP_CONNECTIONS 6
#define MESH_NON_MESH_AP_CONNECTIONS 0
#define MESH_AP_PASS "meshnode"

// Mesh network credentials
#ifndef NETWORK_ENV_HOME
#ifndef MESH_ROUTER_SSID
#define MESH_ROUTER_SSID "n2048"
#endif

#ifndef MESH_ROUTER_PASS
#define MESH_ROUTER_PASS "16377240"
#endif
#else
#ifndef MESH_ROUTER_SSID
#define MESH_ROUTER_SSID "octopuslab"
#endif

#ifndef MESH_ROUTER_PASS
#define MESH_ROUTER_PASS "the-8-lab"
#endif
#endif

// ============================================================================
// UDP Settings
// ============================================================================

#define UDP_PORT 1234
#ifndef TCP_PORT
#define TCP_PORT 1235
#endif

// Visualization server configuration - set via build flags in platformio.ini
#ifndef NETWORK_ENV_HOME
#ifndef VISUALIZATION_IP
#define VISUALIZATION_IP "10.65.5.196"
#endif
#else
#ifndef VISUALIZATION_IP
#define VISUALIZATION_IP "192.168.178.169"
#endif
#endif
#ifndef VISUALIZATION_PORT
#define VISUALIZATION_PORT 1234
#endif

// ============================================================================
// MQTT Configuration
// ============================================================================

// MQTT Configuration - can be disabled if only UDP is used
#ifndef ENABLE_MQTT_VISUALIZATION
#define ENABLE_MQTT_VISUALIZATION 1
#endif

#ifndef NETWORK_ENV_HOME
#ifndef MQTT_BROKER_IP
#define MQTT_BROKER_IP "10.65.5.196"
#endif
#else
#ifndef MQTT_BROKER_IP
#define MQTT_BROKER_IP "192.168.178.169"
#endif
#endif
#define MQTT_BROKER_PORT 1883
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "esp32_mesh_node"
#endif
#define MQTT_TOPOLOGY_TOPIC "mesh/topology"
#define MQTT_STATE_TOPIC "mesh/state"
#define MQTT_COMMAND_TOPIC "mesh/commands"
#define MQTT_UPDATE_INTERVAL_MS 5000
#ifndef MESH_HEALTH_CHECK_INTERVAL_MS
#define MESH_HEALTH_CHECK_INTERVAL_MS 10000
#endif
#ifndef MESH_RECONNECT_ATTEMPT_MS
#define MESH_RECONNECT_ATTEMPT_MS 15000
#endif
#ifndef MESH_RECONNECT_RESTART_MS
#define MESH_RECONNECT_RESTART_MS 60000
#endif
#ifndef MESH_AP_ASSOC_EXPIRE_SECONDS
#define MESH_AP_ASSOC_EXPIRE_SECONDS 30
#endif

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

// IP Configuration - set via build flags in platformio.ini
#ifndef NETWORK_ENV_HOME
#define IP_PREFIX "10.65."
#define IP_SUBNET_MASK "255.240.0.0"
#else
#define IP_PREFIX "192.168."
#define IP_SUBNET_MASK "255.255.240.0"
#endif

// ============================================================================
// Neighbor Settings
// ============================================================================

// Neighbor Settings - can be overridden via build flags.
// Keep this bounded: it is the local neighborhood used by the activation
// kernel, not the total mesh size.
#ifndef MAX_NEIGHBORS
#define MAX_NEIGHBORS 8
#endif
#ifndef RSSI_THRESHOLD
#define RSSI_THRESHOLD -80
#endif
#ifndef ENABLE_BEACON_NEIGHBOR_DISCOVERY
#define ENABLE_BEACON_NEIGHBOR_DISCOVERY 1
#endif
#ifndef ENABLE_LOCAL_NEIGHBOR_STATUS
#define ENABLE_LOCAL_NEIGHBOR_STATUS 1
#endif
#ifndef ENABLE_WIFI_NEIGHBOR_SCAN
#define ENABLE_WIFI_NEIGHBOR_SCAN 0
#endif
#define NEIGHBOR_TIMEOUT_MS 5000
#define BEACON_SCAN_INTERVAL 100
#ifndef MQTT_TOPOLOGY_ROUTE_SAMPLE_LIMIT
#define MQTT_TOPOLOGY_ROUTE_SAMPLE_LIMIT 16
#endif
#ifndef MQTT_TOPOLOGY_NEIGHBOR_SAMPLE_LIMIT
#define MQTT_TOPOLOGY_NEIGHBOR_SAMPLE_LIMIT 8
#endif

// ============================================================================
// State Management Settings
// ============================================================================

#define STATE_UPDATE_INTERVAL 100
#define TEMPERATURE_SCALE 10.0
#ifndef COMMAND_VALUE_HOLD_MS
#define COMMAND_VALUE_HOLD_MS 5000
#endif

// ============================================================================
// LED Configuration
// ============================================================================

#define NUM_LEDS 1
#define DATA_PIN 10
#define TRANSITION_SPEED 0.1
#define LED_STRIP_RMT_RES_HZ 10000000
#define TEMP_READ_INTERVAL_MS 5000

// ============================================================================
// Sensor and Hardware Configuration
// ESP32-C3 GPIO definitions
// ============================================================================

// Sensor pins
#define MMWAVE_PRESENCE_PIN 5

// For ESP32-C3, we use GPIO pins directly
// LED pins for RGB (if using individual LEDs)
#define LED_RED_PIN 1
#define LED_GREEN_PIN 2
#define LED_BLUE_PIN 3

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
#define MSG_TYPE_COMMAND 0x04

#define MESH_COMMAND_TOGGLE_VALUE 0x01
#define MESH_COMMAND_HIGHLIGHT_ON 0x02
#define MESH_COMMAND_HIGHLIGHT_OFF 0x03

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
