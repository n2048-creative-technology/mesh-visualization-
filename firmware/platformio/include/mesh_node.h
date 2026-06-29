/**
 * Mesh Node Header - ESP-IDF Version
 */

#pragma once

#include "config.h"
#include <string.h>
#include "esp_mesh.h"

// Data Structures
typedef struct {
    uint8_t mac[6];
    int8_t rssi;
    uint32_t last_seen;
    uint8_t ip[4];
    bool active;
    bool has_state;
    node_state_t state;
} neighbor_info_t;

// Packed message format for UDP transmission to visualization server
// This matches the format expected by server.js (77 bytes total)
#pragma pack(push, 1)
typedef struct {
    uint8_t version;           // Protocol version (0x01)
    uint8_t msg_type;          // Message type (0x01 = state update)
    uint8_t mac[6];            // Node MAC address
    uint8_t state;             // Node state (0=idle, 1=active, 2=error, 3=booting)
    uint8_t color[3];          // RGB color
    int16_t temperature;       // Temperature ×10
    uint8_t mmwave_presence;   // 0 or 1
    uint32_t mmwave_distance;  // Distance in mm
    uint32_t timestamp;        // Unix timestamp
    struct {
        uint8_t mac[6];        // Neighbor MAC
        int8_t rssi;           // RSSI in dBm
    } neighbors[8];            // Top 8 neighbors by RSSI
    uint16_t checksum;         // Checksum
} udp_message_t;
#pragma pack(pop)

// Internal mesh message format (larger, with additional metadata)
typedef struct {
    uint8_t version;
    uint8_t msg_type;
    uint8_t mac[6];
    node_state_t state;
    uint32_t sequence;
    uint32_t mesh_timestamp;
    neighbor_info_t neighbors[MAX_NEIGHBORS];
    uint16_t checksum;
} mesh_message_t;

// Function Declarations
void init_mesh(void);
void init_wifi(void);
void init_udp(void);
void init_neighbor_list(void);
void update_neighbor_list(uint8_t *mac, int8_t rssi);
void cleanup_inactive_neighbors(void);
neighbor_info_t* get_neighbor_by_mac(uint8_t *mac);
int get_neighbor_count(void);
void fill_neighbor_values(float *values, size_t max_values);
void trigger_neighbor_discovery(void);
void init_state(void);
void update_state(void);
void set_node_state(uint8_t state);
void set_led_color(uint8_t r, uint8_t g, uint8_t b);
void read_sensors(void);
void send_state_to_neighbors(void);
void forward_to_visualization(mesh_message_t *msg);
void broadcast_state(void);
void send_udp_message(uint8_t *dest_ip, uint16_t dest_port, mesh_message_t *msg);
void init_tx_power(void);
void adjust_tx_power(void);
int8_t calculate_required_tx_power(void);
uint16_t calculate_checksum(mesh_message_t *msg);
bool verify_checksum(mesh_message_t *msg);
void mac_to_ip(uint8_t *mac, uint8_t *ip);
void print_mac(uint8_t *mac);
void print_ip(uint8_t *ip);
void platform_init(void);

// Mesh event handler - matches ESP-IDF signature
void mesh_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// WiFi event handler
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// Global Variables
extern mesh_message_t current_message;
extern udp_message_t udp_message;
extern neighbor_info_t neighbor_list[MAX_NEIGHBORS];
extern node_state_t node_state;
extern int8_t current_tx_power;
extern uint8_t node_mac[6];
extern int udp_socket;
extern bool mesh_initialized;
extern bool wifi_initialized;

// Function Declarations for UDP message handling
void prepare_udp_message(udp_message_t *msg);
uint16_t calculate_udp_checksum(udp_message_t *msg);
