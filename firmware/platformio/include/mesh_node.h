/**
 * Mesh Node Header
 * Core mesh network functionality for ESP32
 * PlatformIO compatible for VS Code
 */

#pragma once

#include "config.h"
#include <esp_wifi.h>
#include <esp_mesh.h>
#include <esp_netif.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <string.h>

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Neighbor information structure
 */
typedef struct {
    uint8_t mac[6];      // MAC address
    int8_t rssi;         // Last seen RSSI
    uint32_t last_seen;  // Last beacon timestamp (ms)
    uint8_t ip[4];       // Resolved IP address
    bool active;         // Currently active
} neighbor_info_t;

/**
 * Node state structure
 */
typedef struct {
    uint8_t state;           // Node state (0=idle, 1=active, 2=error, 3=booting)
    uint8_t color[3];        // RGB color
    int16_t temperature;     // Temperature ×10
    uint8_t mmwave_presence; // 0 or 1
    uint32_t mmwave_distance; // Distance in mm
    uint32_t timestamp;      // Unix timestamp
} node_state_t;

/**
 * UDP message structure for state updates
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t version;         // Protocol version
    uint8_t msg_type;        // Message type
    uint8_t mac[6];          // Source node MAC
    node_state_t state;      // Node state
    neighbor_info_t neighbors[MAX_NEIGHBORS]; // Top 8 neighbors
    uint16_t checksum;       // Simple checksum
} mesh_message_t;
#pragma pack(pop)

// ============================================================================
// Function Declarations
// ============================================================================

// Initialization
void init_mesh(void);
void init_wifi(void);
void init_udp(void);

// Neighbor management
void init_neighbor_list(void);
void update_neighbor_list(uint8_t *mac, int8_t rssi);
void cleanup_inactive_neighbors(void);
neighbor_info_t* get_neighbor_by_mac(uint8_t *mac);
int get_neighbor_count(void);

// State management
void init_state(void);
void update_state(void);
void set_led_color(uint8_t r, uint8_t g, uint8_t b);
void read_sensors(void);

// Communication
void send_state_to_neighbors(void);
void forward_to_visualization(mesh_message_t *msg);
void broadcast_state(void);
void send_udp_message(uint8_t *dest_ip, uint16_t dest_port, mesh_message_t *msg);

// TX Power management
void init_tx_power(void);
void adjust_tx_power(void);
int8_t calculate_required_tx_power(void);

// Utility functions
uint16_t calculate_checksum(mesh_message_t *msg);
bool verify_checksum(mesh_message_t *msg);
void mac_to_ip(uint8_t *mac, uint8_t *ip);
void print_mac(uint8_t *mac);
void print_ip(uint8_t *ip);

// Platform-specific initialization
void platform_init(void);

// Callbacks
esp_err_t mesh_event_handler(mesh_event_t event);
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// ============================================================================
// Global Variables (extern)
// ============================================================================

extern mesh_message_t current_message;
extern neighbor_info_t neighbor_list[MAX_NEIGHBORS];
extern node_state_t node_state;
extern int8_t current_tx_power;
extern uint8_t node_mac[6];
extern int udp_socket;
extern bool mesh_initialized;
extern bool wifi_initialized;
