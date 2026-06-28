/**
 * Mesh Node Implementation - ESP-IDF Version
 * Neighbor discovery via RSSI thresholding
 */

#include "mesh_node.h"
#include "config.h"
#include "mqtt_handler.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"

static const char *TAG = "MESH_NODE";

// Forward declaration for WiFi scan callback
static void wifi_scan_done_callback(void *arg, esp_event_base_t event_base, 
                                     int32_t event_id, void *event_data);

// Global variables - these are declared as extern in the header
mesh_message_t current_message;
neighbor_info_t neighbor_list[MAX_NEIGHBORS];
int8_t current_tx_power = 8;
int udp_socket = -1;
bool mesh_initialized = false;
bool wifi_initialized = false;

// Timer handles
void* state_update_timer = NULL;
void* neighbor_cleanup_timer = NULL;

/**
 * Platform-specific initialization
 */
void platform_init(void) {
    ESP_LOGI(TAG, "Initializing platform: %s", PLATFORM_NAME);
    ESP_LOGI(TAG, "Using WiFi channel: %d", WIFI_CHANNEL);
}

/**
 * Initialize WiFi for mesh networking
 */
void init_wifi(void) {
    ESP_LOGI(TAG, "Initializing WiFi");
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_t *netif_sta = NULL;
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));
    
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    
    // Register WiFi event handler for scan completion
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, 
                                                  &wifi_scan_done_callback, NULL));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    uint8_t local_mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
    memcpy(node_mac, local_mac, 6);
    
    // Initialize neighbor list
    init_neighbor_list();
    
    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized with neighbor discovery support");
}

/**
 * Initialize ESP-WiFi-Mesh
 */
void init_mesh(void) {
    ESP_LOGI(TAG, "Initializing ESP-WiFi-Mesh");
    
    ESP_ERROR_CHECK(esp_mesh_init());
    
    ESP_ERROR_CHECK(esp_mesh_set_topology(MESH_TOPO_TREE));
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(MESH_MAX_HOPS));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(MESH_VOTE_PERCENT));
    ESP_ERROR_CHECK(esp_mesh_disable_ps());
    
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    cfg.channel = WIFI_CHANNEL;
    memcpy((uint8_t *) &cfg.router.ssid, MESH_ROUTER_SSID, strlen(MESH_ROUTER_SSID));
    memcpy((uint8_t *) &cfg.router.password, MESH_ROUTER_PASS, strlen(MESH_ROUTER_PASS));
    memcpy((uint8_t *) &cfg.mesh_id, (uint8_t*)"glow_mesh", 10);
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    
    ESP_ERROR_CHECK(esp_mesh_start());
    
    mesh_initialized = true;
    ESP_LOGI(TAG, "ESP-WiFi-Mesh initialized");
    
    // Initialize MQTT for visualization
    #ifdef ENABLE_MQTT_VISUALIZATION
    init_mqtt();
    #endif
}

/**
 * Initialize UDP socket
 */
void init_udp(void) {
    ESP_LOGI(TAG, "Initializing UDP socket");
    
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: %d", errno);
        return;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(udp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind UDP socket: %d", errno);
        close(udp_socket);
        udp_socket = -1;
        return;
    }
    
    ESP_LOGI(TAG, "UDP socket initialized on port %d", UDP_PORT);
}

/**
 * Initialize neighbor list
 */
void init_neighbor_list(void) {
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        neighbor_list[i].active = false;
        neighbor_list[i].rssi = -127;
        neighbor_list[i].last_seen = 0;
        memset(neighbor_list[i].mac, 0, 6);
        memset(neighbor_list[i].ip, 0, 4);
    }
    ESP_LOGI(TAG, "Neighbor list initialized for %d neighbors", MAX_NEIGHBORS);
}

/**
 * Find a neighbor by MAC address
 */
neighbor_info_t* get_neighbor_by_mac(uint8_t *mac) {
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && memcmp(neighbor_list[i].mac, mac, 6) == 0) {
            return &neighbor_list[i];
        }
    }
    return NULL;
}

/**
 * Get the number of active neighbors
 */
int get_neighbor_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active) {
            count++;
        }
    }
    return count;
}

/**
 * Update or add a neighbor to the list
 * Uses RSSI thresholding: only neighbors with RSSI >= RSSI_THRESHOLD are kept
 */
void update_neighbor_list(uint8_t *mac, int8_t rssi) {
    uint32_t now = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
    
    // Skip if RSSI is below threshold (unless already in list)
    if (rssi < RSSI_THRESHOLD) {
        // If already in list, just update last_seen and keep it
        // This allows temporary weak signals to maintain connection
        neighbor_info_t *existing = get_neighbor_by_mac(mac);
        if (existing) {
            existing->last_seen = now;
        }
        return;
    }
    
    // Check if neighbor already exists
    neighbor_info_t *existing = get_neighbor_by_mac(mac);
    if (existing) {
        // Update existing neighbor
        existing->rssi = rssi;
        existing->last_seen = now;
        ESP_LOGD(TAG, "Updated neighbor: " MACSTR " RSSI: %d", MAC2STR(mac), rssi);
        return;
    }
    
    // Find an empty slot
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (!neighbor_list[i].active) {
            neighbor_list[i].active = true;
            memcpy(neighbor_list[i].mac, mac, 6);
            neighbor_list[i].rssi = rssi;
            neighbor_list[i].last_seen = now;
            memset(neighbor_list[i].ip, 0, 4);
            ESP_LOGI(TAG, "Added new neighbor: " MACSTR " RSSI: %d", MAC2STR(mac), rssi);
            return;
        }
    }
    
    // No empty slots - find the weakest/oldest neighbor and replace it
    int weakest_index = 0;
    int8_t weakest_rssi = neighbor_list[0].rssi;
    uint32_t oldest_time = neighbor_list[0].last_seen;
    
    for (int i = 1; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].rssi < weakest_rssi) {
            weakest_rssi = neighbor_list[i].rssi;
            weakest_index = i;
        }
        if (neighbor_list[i].last_seen < oldest_time) {
            oldest_time = neighbor_list[i].last_seen;
            weakest_index = i;
        }
    }
    
    // Replace the weakest/oldest
    neighbor_list[weakest_index].active = true;
    memcpy(neighbor_list[weakest_index].mac, mac, 6);
    neighbor_list[weakest_index].rssi = rssi;
    neighbor_list[weakest_index].last_seen = now;
    memset(neighbor_list[weakest_index].ip, 0, 4);
    ESP_LOGW(TAG, "Replaced weakest neighbor with: " MACSTR " RSSI: %d", MAC2STR(mac), rssi);
}

/**
 * Remove neighbors that haven't been seen for NEIGHBOR_TIMEOUT_MS
 */
void cleanup_inactive_neighbors(void) {
    uint32_t now = esp_timer_get_time() / 1000;
    int count_before = get_neighbor_count();
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active) {
            uint32_t time_since_seen = now - neighbor_list[i].last_seen;
            if (time_since_seen > NEIGHBOR_TIMEOUT_MS) {
                ESP_LOGI(TAG, "Removed inactive neighbor: " MACSTR " (last seen %lu ms ago)", 
                         MAC2STR(neighbor_list[i].mac), time_since_seen);
                neighbor_list[i].active = false;
                neighbor_list[i].rssi = -127;
            }
        }
    }
    
    int count_after = get_neighbor_count();
    if (count_before != count_after) {
        ESP_LOGI(TAG, "Neighbor cleanup: %d -> %d active neighbors", count_before, count_after);
    }
}

/**
 * WiFi scan callback for neighbor discovery
 */
static void wifi_scan_done_callback(void *arg, esp_event_base_t event_base, 
                                     int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        uint16_t ap_count = 0;
        wifi_ap_record_t ap_records[MAX_NEIGHBORS];
        
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
        
        ESP_LOGD(TAG, "WiFi scan found %d APs", ap_count);
        
        for (int i = 0; i < ap_count && i < MAX_NEIGHBORS; i++) {
            // Skip our own MAC and AP entries with invalid RSSI
            if (ap_records[i].rssi == 0 || ap_records[i].rssi == -128) {
                continue;
            }
            
            // Update neighbor list with this AP
            update_neighbor_list(ap_records[i].bssid, ap_records[i].rssi);
        }
        
        // Cleanup inactive neighbors after scan
        cleanup_inactive_neighbors();
        
        // Log neighbor count
        ESP_LOGI(TAG, "Total active neighbors: %d", get_neighbor_count());
        
        // Publish updated topology via MQTT
        #ifdef ENABLE_MQTT_VISUALIZATION
        mqtt_publish_topology_and_state();
        #endif
    }
}

/**
 * Trigger a WiFi scan for neighbor discovery
 * This is called periodically to update the neighbor list
 */
void trigger_neighbor_discovery(void) {
    if (!wifi_initialized) {
        ESP_LOGW(TAG, "WiFi not initialized, cannot scan for neighbors");
        return;
    }
    
    // Set scan configuration
    wifi_scan_config_t scan_config = {
        .ssid = NULL,  // Scan all SSIDs
        .bssid = NULL, // Scan all BSSIDs
        .channel = WIFI_CHANNEL, // Scan only our channel
        .show_hidden = true
    };
    
    ESP_LOGI(TAG, "Starting neighbor discovery scan on channel %d", WIFI_CHANNEL);
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
}

/**
 * Initialize TX power
 */
void init_tx_power(void) {
    current_tx_power = 8;
}

/**
 * Stub functions for compilation
 */
void send_state_to_neighbors(void) {}
void broadcast_state(void) {}
void forward_to_visualization(mesh_message_t *msg) {}
void send_udp_message(uint8_t *dest_ip, uint16_t dest_port, mesh_message_t *msg) {}

void set_node_state(uint8_t state) {
    node_state.state = state;
    ESP_LOGI(TAG, "Node state set to: %d", state);
}

uint16_t calculate_checksum(mesh_message_t *msg) { return 0; }
bool verify_checksum(mesh_message_t *msg) { return true; }
void mac_to_ip(uint8_t *mac, uint8_t *ip) {}
void print_mac(uint8_t *mac) {}
void print_ip(uint8_t *ip) {}

// WiFi event handler - note: mesh_event_handler is defined in main.cpp
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    // Forward to the main mesh event handler
    extern void mesh_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    if (event_base == MESH_EVENT) {
        mesh_event_handler(arg, event_base, event_id, event_data);
    }
}

void adjust_tx_power(void) {}
int8_t calculate_required_tx_power(void) { return 0; }

/**
 * Initialize state - defined in state_manager.cpp
 */
void init_state(void);
