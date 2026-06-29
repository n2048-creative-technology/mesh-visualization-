/**
 * Mesh Node Implementation - ESP-IDF Version
 * Neighbor discovery via RSSI thresholding
 * State sharing via mesh network
 * UDP messaging to visualization tool
 */

#include "mesh_node.h"
#include "config.h"
#include "mqtt_handler.h"
#include "state_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_mesh.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include <stddef.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MESH_NODE";

// Global variables
mesh_message_t current_message;
udp_message_t udp_message;
neighbor_info_t neighbor_list[MAX_NEIGHBORS];
int8_t current_tx_power = 8;
int udp_socket = -1;
bool mesh_initialized = false;
bool wifi_initialized = false;
mesh_addr_t current_root_addr = {};
bool has_current_root_addr = false;

// Message sequence number
static uint32_t message_sequence = 0;
static const uint8_t mesh_id[6] = {0x47, 0x4c, 0x4f, 0x57, 0x20, 0x01};
static bool mesh_stack_initialized = false;
static bool mesh_rx_task_started = false;

static void mesh_receive_task(void *arg);

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
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (WIFI_CHANNEL > 0) {
        ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    }
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

    if (!mesh_stack_initialized) {
        esp_err_t init_err = esp_mesh_init();
        if (init_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize ESP-WiFi-Mesh: %s", esp_err_to_name(init_err));
            return;
        }
        mesh_stack_initialized = true;
    }
    
    ESP_ERROR_CHECK(esp_mesh_set_topology(MESH_TOPO_TREE));
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(MESH_MAX_HOPS));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(MESH_VOTE_PERCENT));
    ESP_ERROR_CHECK(esp_mesh_disable_ps());
    
    mesh_cfg_t cfg = {};
    cfg.crypto_funcs = &g_wifi_default_mesh_crypto_funcs;
    cfg.channel = WIFI_CHANNEL;
    cfg.router.ssid_len = strlen(MESH_ROUTER_SSID);
    memcpy(cfg.router.ssid, MESH_ROUTER_SSID, MIN(sizeof(cfg.router.ssid), cfg.router.ssid_len));
    memcpy(cfg.router.password, MESH_ROUTER_PASS, MIN(sizeof(cfg.router.password), strlen(MESH_ROUTER_PASS)));
    memcpy(cfg.mesh_ap.password, MESH_AP_PASS, MIN(sizeof(cfg.mesh_ap.password), strlen(MESH_AP_PASS)));
    cfg.mesh_ap.max_connection = MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = MESH_NON_MESH_AP_CONNECTIONS;
    memcpy(cfg.mesh_id.addr, mesh_id, sizeof(mesh_id));

    esp_err_t err = esp_mesh_set_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ESP-WiFi-Mesh: %s", esp_err_to_name(err));
        return;
    }
    
    err = esp_mesh_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start ESP-WiFi-Mesh: %s", esp_err_to_name(err));
        return;
    }
    
    mesh_initialized = true;
    if (!mesh_rx_task_started) {
        xTaskCreate(mesh_receive_task, "mesh_rx", 6144, NULL, 5, NULL);
        mesh_rx_task_started = true;
    }
    ESP_LOGI(TAG, "ESP-WiFi-Mesh initialized");
}

/**
 * Initialize UDP socket
 */
void init_udp(void) {
    if (udp_socket >= 0) {
        ESP_LOGI(TAG, "UDP socket already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing UDP socket on port %d", UDP_PORT);
    
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: %d", errno);
        return;
    }
    
    // Set socket options for broadcast and reuse
    int opt = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_BROADCAST: %d", errno);
    }
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: %d", errno);
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
        neighbor_list[i].has_state = false;
        neighbor_list[i].rssi = -127;
        neighbor_list[i].last_seen = 0;
        memset(neighbor_list[i].mac, 0, 6);
        memset(neighbor_list[i].ip, 0, 4);
        memset(&neighbor_list[i].state, 0, sizeof(node_state_t));
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

void fill_neighbor_values(float *values, size_t max_values) {
    if (values == NULL) return;

    size_t written = 0;
    for (int i = 0; i < MAX_NEIGHBORS && written < max_values; i++) {
        if (neighbor_list[i].active && neighbor_list[i].has_state) {
            values[written++] = neighbor_list[i].state.value ? 1.0f : 0.0f;
        }
    }

    while (written < max_values) {
        values[written++] = 0.0f;
    }
}

/**
 * Update or add a neighbor to the list
 * Uses RSSI thresholding: only neighbors with RSSI >= RSSI_THRESHOLD are kept
 * Maintains top MAX_NEIGHBORS by RSSI (strongest signals)
 */
void update_neighbor_list(uint8_t *mac, int8_t rssi) {
    uint32_t now = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
    
    // Skip our own MAC
    if (memcmp(mac, node_mac, 6) == 0) {
        return;
    }
    
    // Skip if RSSI is below threshold (unless already in list)
    if (rssi < RSSI_THRESHOLD) {
        // If already in list, just update last_seen and keep it
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
            neighbor_list[i].has_state = false;
            memcpy(neighbor_list[i].mac, mac, 6);
            neighbor_list[i].rssi = rssi;
            neighbor_list[i].last_seen = now;
            memset(neighbor_list[i].ip, 0, 4);
            ESP_LOGI(TAG, "Added new neighbor: " MACSTR " RSSI: %d", MAC2STR(mac), rssi);
            return;
        }
    }
    
    // No empty slots - find the weakest neighbor and replace it if new one is stronger
    int weakest_index = 0;
    int8_t weakest_rssi = neighbor_list[0].rssi;
    
    for (int i = 1; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].rssi < weakest_rssi) {
            weakest_rssi = neighbor_list[i].rssi;
            weakest_index = i;
        }
    }
    
    // Only replace if new neighbor has better RSSI
    if (rssi > weakest_rssi) {
        neighbor_list[weakest_index].active = true;
        neighbor_list[weakest_index].has_state = false;
        memcpy(neighbor_list[weakest_index].mac, mac, 6);
        neighbor_list[weakest_index].rssi = rssi;
        neighbor_list[weakest_index].last_seen = now;
        memset(neighbor_list[weakest_index].ip, 0, 4);
        ESP_LOGI(TAG, "Replaced weakest neighbor with: " MACSTR " RSSI: %d (was %d)", 
                 MAC2STR(mac), rssi, weakest_rssi);
    }
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
                neighbor_list[i].has_state = false;
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
        wifi_ap_record_t ap_records[MAX_NEIGHBORS * 2]; // Scan more than we need
        
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
        
        ESP_LOGD(TAG, "WiFi scan found %d APs", ap_count);
        
        for (int i = 0; i < ap_count; i++) {
            // Skip our own MAC and AP entries with invalid RSSI
            if (ap_records[i].rssi == 0 || ap_records[i].rssi == -128) {
                continue;
            }
            
            // Skip if this is the router AP (we're interested in mesh nodes)
            // Check if SSID matches our mesh router SSID
            if (strlen(MESH_ROUTER_SSID) > 0 && 
                strncmp((char*)ap_records[i].ssid, MESH_ROUTER_SSID, strlen(MESH_ROUTER_SSID)) == 0) {
                // This might be the router itself, skip it
                continue;
            }
            
            // Update neighbor list with this AP (mesh node)
            update_neighbor_list(ap_records[i].bssid, ap_records[i].rssi);
        }
        
        // Cleanup inactive neighbors after scan
        cleanup_inactive_neighbors();
        
        // Log neighbor count
        ESP_LOGI(TAG, "Total active neighbors: %d", get_neighbor_count());
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
    wifi_scan_config_t scan_config = {};
    scan_config.ssid = NULL;
    scan_config.bssid = NULL;
    scan_config.channel = WIFI_CHANNEL;
    scan_config.show_hidden = true;
    
    ESP_LOGD(TAG, "Starting neighbor discovery scan on channel %d", WIFI_CHANNEL);
    
    // Register scan callback if not already registered
    static bool scan_callback_registered = false;
    if (!scan_callback_registered) {
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, 
                                                  &wifi_scan_done_callback, NULL));
        scan_callback_registered = true;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
}

/**
 * Calculate checksum for a mesh message (sum of all bytes except checksum field)
 */
uint16_t calculate_checksum(mesh_message_t *msg) {
    uint16_t checksum = 0;
    uint8_t *data = (uint8_t *)msg;
    size_t len = offsetof(mesh_message_t, checksum); // Exclude checksum field and trailing padding
    
    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    
    return checksum;
}

/**
 * Calculate checksum for a UDP message (sum of all bytes except checksum field)
 * This matches the checksum calculation in server.js
 */
uint16_t calculate_udp_checksum(udp_message_t *msg) {
    uint16_t checksum = 0;
    uint8_t *data = (uint8_t *)msg;
    size_t len = offsetof(udp_message_t, checksum); // Exclude checksum field
    
    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    
    return checksum;
}

/**
 * Prepare UDP message for visualization server
 * Converts internal state to the UDP message format expected by server.js
 */
void prepare_udp_message(udp_message_t *msg) {
    memset(msg, 0, sizeof(udp_message_t));
    
    msg->version = PROTOCOL_VERSION;
    msg->msg_type = MSG_TYPE_STATE_UPDATE;
    memcpy(msg->mac, node_mac, 6);
    
    // Copy state from node_state
    msg->state = node_state.state;
    memcpy(msg->color, node_state.color, 3);
    msg->temperature = node_state.temperature;
    msg->mmwave_presence = node_state.mmwave_presence;
    msg->mmwave_distance = node_state.mmwave_distance;
    msg->timestamp = node_state.timestamp;
    
    // Copy neighbor information (only top 8 by RSSI)
    int neighbor_count = get_neighbor_count();
    int copy_count = neighbor_count < 8 ? neighbor_count : 8;
    
    // Sort neighbors by RSSI (strongest first)
    for (int i = 0; i < copy_count; i++) {
        int best_index = -1;
        int8_t best_rssi = -127;
        
        for (int j = 0; j < MAX_NEIGHBORS; j++) {
            if (neighbor_list[j].active && neighbor_list[j].rssi > best_rssi) {
                // Check if this neighbor is already in the top 8
                bool already_included = false;
                for (int k = 0; k < i; k++) {
                    if (memcmp(neighbor_list[j].mac, msg->neighbors[k].mac, 6) == 0) {
                        already_included = true;
                        break;
                    }
                }
                
                if (!already_included) {
                    best_rssi = neighbor_list[j].rssi;
                    best_index = j;
                }
            }
        }
        
        if (best_index >= 0) {
            memcpy(msg->neighbors[i].mac, neighbor_list[best_index].mac, 6);
            msg->neighbors[i].rssi = neighbor_list[best_index].rssi;
        } else {
            // Clear this neighbor slot
            memset(msg->neighbors[i].mac, 0, 6);
            msg->neighbors[i].rssi = -127;
        }
    }
    
    // Clear remaining neighbor slots
    for (int i = copy_count; i < 8; i++) {
        memset(msg->neighbors[i].mac, 0, 6);
        msg->neighbors[i].rssi = -127;
    }
    
    // Calculate checksum
    msg->checksum = calculate_udp_checksum(msg);
}

/**
 * Verify checksum of a message
 */
bool verify_checksum(mesh_message_t *msg) {
    uint16_t calculated = calculate_checksum(msg);
    return calculated == msg->checksum;
}

static void mesh_receive_task(void *arg) {
    mesh_addr_t from;
    mesh_message_t msg;
    mesh_data_t data;
    int flag = 0;

    data.data = (uint8_t *)&msg;
    data.size = sizeof(msg);

    while (true) {
        memset(&from, 0, sizeof(from));
        memset(&msg, 0, sizeof(msg));
        data.size = sizeof(msg);

        esp_err_t err = esp_mesh_recv(&from, &data, pdMS_TO_TICKS(1000), &flag, NULL, 0);
        if (err == ESP_ERR_MESH_TIMEOUT) {
            continue;
        }
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "esp_mesh_recv failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        if (data.size != sizeof(mesh_message_t)) {
            ESP_LOGW(TAG, "Ignoring unexpected mesh payload size: %d", data.size);
            continue;
        }
        if (msg.version != PROTOCOL_VERSION || msg.msg_type != MSG_TYPE_STATE_UPDATE) {
            ESP_LOGW(TAG, "Ignoring unsupported mesh message version=%u type=%u",
                     msg.version, msg.msg_type);
            continue;
        }
        if (!verify_checksum(&msg)) {
            ESP_LOGW(TAG, "Ignoring mesh message with invalid checksum from " MACSTR,
                     MAC2STR(msg.mac));
            continue;
        }

        update_neighbor_list(msg.mac, -50);
        neighbor_info_t *neighbor = get_neighbor_by_mac(msg.mac);
        if (neighbor) {
            neighbor->state = msg.state;
            neighbor->has_state = true;
            neighbor->last_seen = esp_timer_get_time() / 1000;
        }

        adopt_neighbor_rules(&msg.state);

        if (esp_mesh_is_root()) {
            mqtt_publish_node_state(msg.mac, &msg.state);
            forward_to_visualization(&msg);
        }
    }
}

/**
 * Send state to all neighbors via mesh
 * Uses ESP-WiFi-Mesh to send message to neighbor nodes
 */
void send_state_to_neighbors(void) {
    if (!mesh_initialized) {
        ESP_LOGW(TAG, "Mesh not initialized, cannot send to neighbors");
        return;
    }
    
    // Build the message
    mesh_message_t msg = {};
    msg.version = PROTOCOL_VERSION;
    msg.msg_type = MSG_TYPE_STATE_UPDATE;
    memcpy(msg.mac, node_mac, 6);
    msg.state = node_state;
    msg.sequence = message_sequence++;
    msg.mesh_timestamp = esp_timer_get_time() / 1000;
    
    // Copy neighbor information
    int neighbor_count = get_neighbor_count();
    int copy_count = neighbor_count < MAX_NEIGHBORS ? neighbor_count : MAX_NEIGHBORS;
    
    for (int i = 0; i < copy_count; i++) {
        if (neighbor_list[i].active) {
            memcpy(msg.neighbors[i].mac, neighbor_list[i].mac, 6);
            msg.neighbors[i].rssi = neighbor_list[i].rssi;
            msg.neighbors[i].last_seen = neighbor_list[i].last_seen;
            msg.neighbors[i].active = true;
        } else {
            memset(msg.neighbors[i].mac, 0, 6);
            msg.neighbors[i].rssi = -127;
            msg.neighbors[i].last_seen = 0;
            msg.neighbors[i].active = false;
        }
    }
    
    // Clear remaining neighbors
    for (int i = copy_count; i < MAX_NEIGHBORS; i++) {
        memset(msg.neighbors[i].mac, 0, 6);
        msg.neighbors[i].rssi = -127;
        msg.neighbors[i].last_seen = 0;
        msg.neighbors[i].active = false;
    }
    
    msg.checksum = calculate_checksum(&msg);

    mesh_data_t mesh_data;
    mesh_data.data = (uint8_t*)&msg;
    mesh_data.size = sizeof(mesh_message_t);
    mesh_data.proto = MESH_PROTO_BIN;
    mesh_data.tos = MESH_TOS_P2P;

    if (!esp_mesh_is_root()) {
        mesh_addr_t *dest = has_current_root_addr ? &current_root_addr : NULL;
        int flag = has_current_root_addr ? MESH_DATA_P2P : MESH_DATA_TODS;
        esp_err_t err = esp_mesh_send(dest, &mesh_data, flag, NULL, 0);
        if (err == ESP_OK) {
            ESP_LOGD(TAG, "State sent upward to mesh root");
        } else {
            ESP_LOGW(TAG, "Failed to send state upward to root: %s", esp_err_to_name(err));
        }
        return;
    }
    
    // Send to each active neighbor
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active) {
            // Use mesh API to send data
            mesh_addr_t dest_addr;
            memcpy(dest_addr.addr, neighbor_list[i].mac, 6);

            esp_err_t err = esp_mesh_send(&dest_addr, &mesh_data, 
                                         MESH_DATA_P2P, NULL, 0);
            
            if (err == ESP_OK) {
                ESP_LOGD(TAG, "State sent to neighbor " MACSTR, MAC2STR(neighbor_list[i].mac));
            } else {
                ESP_LOGW(TAG, "Failed to send to neighbor " MACSTR ": %s", 
                         MAC2STR(neighbor_list[i].mac), esp_err_to_name(err));
            }
        }
    }
}

/**
 * Broadcast state to all nodes in the mesh
 */
void broadcast_state(void) {
    if (!mesh_initialized) {
        ESP_LOGW(TAG, "Mesh not initialized, cannot broadcast");
        return;
    }
    
    // Build the message
    mesh_message_t msg = {};
    msg.version = PROTOCOL_VERSION;
    msg.msg_type = MSG_TYPE_STATE_UPDATE;
    memcpy(msg.mac, node_mac, 6);
    msg.state = node_state;
    msg.sequence = message_sequence++;
    msg.mesh_timestamp = esp_timer_get_time() / 1000;
    
    // Copy neighbor information
    int neighbor_count = get_neighbor_count();
    int copy_count = neighbor_count < MAX_NEIGHBORS ? neighbor_count : MAX_NEIGHBORS;
    
    for (int i = 0; i < copy_count; i++) {
        if (neighbor_list[i].active) {
            memcpy(msg.neighbors[i].mac, neighbor_list[i].mac, 6);
            msg.neighbors[i].rssi = neighbor_list[i].rssi;
            msg.neighbors[i].last_seen = neighbor_list[i].last_seen;
            msg.neighbors[i].active = true;
        } else {
            memset(msg.neighbors[i].mac, 0, 6);
            msg.neighbors[i].rssi = -127;
            msg.neighbors[i].last_seen = 0;
            msg.neighbors[i].active = false;
        }
    }
    
    for (int i = copy_count; i < MAX_NEIGHBORS; i++) {
        memset(msg.neighbors[i].mac, 0, 6);
        msg.neighbors[i].rssi = -127;
    }
    
    msg.checksum = calculate_checksum(&msg);
    
    // Broadcast to all nodes
    mesh_data_t mesh_data;
    mesh_data.data = (uint8_t*)&msg;
    mesh_data.size = sizeof(mesh_message_t);
    mesh_data.proto = MESH_PROTO_BIN; // Binary protocol
    
    // For broadcast, use NULL as destination
    esp_err_t err = esp_mesh_send(NULL, &mesh_data, 
                                 MESH_DATA_P2P, NULL, 0);
    
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "State broadcast to all nodes");
    } else {
        ESP_LOGW(TAG, "Failed to broadcast state: %s", esp_err_to_name(err));
    }
}

/**
 * Send UDP message to a specific IP and port
 */
void send_udp_message(uint8_t *dest_ip, uint16_t dest_port, mesh_message_t *msg) {
    if (udp_socket < 0) {
        ESP_LOGW(TAG, "UDP socket not initialized");
        return;
    }
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    memcpy(&dest_addr.sin_addr.s_addr, dest_ip, 4);
    
    ssize_t bytes_sent = sendto(udp_socket, msg, sizeof(mesh_message_t), 0,
                               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (bytes_sent < 0) {
        ESP_LOGE(TAG, "Failed to send UDP message: %d", errno);
    } else {
        ESP_LOGD(TAG, "UDP message sent (%d bytes) to %d.%d.%d.%d:%d", 
                 bytes_sent,
                 (dest_addr.sin_addr.s_addr >> 0) & 0xFF,
                 (dest_addr.sin_addr.s_addr >> 8) & 0xFF,
                 (dest_addr.sin_addr.s_addr >> 16) & 0xFF,
                 (dest_addr.sin_addr.s_addr >> 24) & 0xFF,
                 ntohs(dest_addr.sin_port));
    }
}

/**
 * Forward message to visualization server
 */
void forward_to_visualization(mesh_message_t *msg) {
    if (udp_socket < 0) {
        ESP_LOGW(TAG, "UDP socket not initialized");
        return;
    }
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(VISUALIZATION_PORT);
    inet_pton(AF_INET, VISUALIZATION_IP, &dest_addr.sin_addr.s_addr);
    
    ssize_t bytes_sent = sendto(udp_socket, msg, sizeof(mesh_message_t), 0,
                               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (bytes_sent < 0) {
        ESP_LOGE(TAG, "Failed to forward to visualization: %d", errno);
    } else {
        ESP_LOGD(TAG, "Forwarded message to visualization server");
    }
}

/**
 * Convert MAC to IP address (for mesh network)
 * This creates an IP in the format: IP_PREFIX + last two bytes of MAC
 */
void mac_to_ip(uint8_t *mac, uint8_t *ip) {
    // Simple mapping for mesh networking
    // Use the IP_PREFIX to determine the first part
    // Then use last two bytes of MAC as last two bytes of IP
    
    // Parse IP_PREFIX (e.g., "10.65." or "192.168.")
    // For now, use a simple hardcoded approach based on NETWORK_ENV_HOME
    #ifndef NETWORK_ENV_HOME
    // Studio environment (10.65.x.x)
    ip[0] = 10;
    ip[1] = 65;
    #else
    // Home environment (192.168.x.x)
    ip[0] = 192;
    ip[1] = 168;
    #endif
    
    // Use last two bytes of MAC as last two bytes of IP
    ip[2] = mac[4];
    ip[3] = mac[5];
}

/**
 * Print MAC address
 */
void print_mac(uint8_t *mac) {
    ESP_LOGI(TAG, "MAC: " MACSTR, MAC2STR(mac));
}

/**
 * Print IP address
 */
void print_ip(uint8_t *ip) {
    ESP_LOGI(TAG, "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

/**
 * Initialize TX power
 */
void init_tx_power(void) {
    current_tx_power = 8;
}

/**
 * Adjust TX power based on weakest neighbor RSSI
 */
void adjust_tx_power(void) {
    if (!wifi_initialized) return;
    
    int8_t weakest_rssi = -127;
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && neighbor_list[i].rssi > weakest_rssi) {
            weakest_rssi = neighbor_list[i].rssi;
        }
    }
    
    if (weakest_rssi == -127) {
        // No neighbors, use default power
        current_tx_power = 8;
        return;
    }
    
    // Adjust power to target RSSI
    int8_t target_rssi = TARGET_RSSI;
    int8_t rssi_diff = target_rssi - weakest_rssi;
    
    // Each 3dB change in TX power roughly changes RSSI by 1dB at receiver
    // This is a simplification - actual relationship depends on distance, obstacles, etc.
    int8_t new_power = current_tx_power + (rssi_diff / 2);
    
    // Clamp to valid range
    new_power = CLAMP(new_power, MIN_TX_POWER, MAX_TX_POWER);
    
    if (new_power != current_tx_power) {
        current_tx_power = new_power;
        ESP_LOGI(TAG, "Adjusted TX power to %d dBm (weakest neighbor RSSI: %d)", 
                 current_tx_power, weakest_rssi);
        
        // Apply the new TX power
        esp_wifi_set_max_tx_power(new_power);
    }
}

/**
 * Calculate required TX power for a specific neighbor
 */
int8_t calculate_required_tx_power(void) {
    int8_t weakest_rssi = -127;
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && neighbor_list[i].rssi > weakest_rssi) {
            weakest_rssi = neighbor_list[i].rssi;
        }
    }
    
    if (weakest_rssi == -127) {
        return 8; // Default power
    }
    
    // Calculate required power to reach target RSSI
    int8_t target_rssi = TARGET_RSSI;
    int8_t rssi_diff = target_rssi - weakest_rssi;
    int8_t required_power = 8 + (rssi_diff / 2);
    
    return CLAMP(required_power, MIN_TX_POWER, MAX_TX_POWER);
}

/**
 * WiFi event handler
 */
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == MESH_EVENT) {
        mesh_event_handler(arg, event_base, event_id, event_data);
    }
}

/**
 * Stub functions for compatibility
 */
void set_node_state(uint8_t state) {
    node_state.state = state;
    ESP_LOGI(TAG, "Node state set to: %d", state);
}
