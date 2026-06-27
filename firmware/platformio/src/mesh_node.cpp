/**
 * Mesh Node Implementation
 * Core mesh network functionality for ESP32
 * PlatformIO compatible for VS Code
 * Supports: ESP32-C3 (2.4 GHz), ESP32-C5 (5 GHz), Generic ESP32
 */

#include "mesh_node.h"
#include "beacon_monitor.h"
#include "state_manager.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_mesh.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <string.h>
#include <time.h>

static const char *TAG = "MESH_NODE";

// Global variables
mesh_message_t current_message;
neighbor_info_t neighbor_list[MAX_NEIGHBORS];
node_state_t node_state;
int8_t current_tx_power = 8; // Default: 0 dBm (8 = 0dBm in ESP32)
uint8_t node_mac[6] = {0};
int udp_socket = -1;
bool mesh_initialized = false;
bool wifi_initialized = false;

// Timer handles
esp_timer_handle_t state_update_timer = NULL;
esp_timer_handle_t neighbor_cleanup_timer = NULL;

// ============================================================================
// Platform-Specific Initialization
// ============================================================================

/**
 * Platform-specific initialization
 */
void platform_init(void) {
    ESP_LOGI(TAG, "Initializing platform: %s", PLATFORM_NAME);
    
    // Platform-specific WiFi configuration
    #if ESP_PLATFORM == ESP32_C3
        ESP_LOGI(TAG, "Configuring for ESP32-C3 (2.4 GHz only)");
    #elif ESP_PLATFORM == ESP32_C5
        ESP_LOGI(TAG, "Configuring for ESP32-C5 (2.4 GHz + 5 GHz)");
    #elif ESP_PLATFORM == ESP32 || ESP_PLATFORM == ESP32_GENERIC
        ESP_LOGI(TAG, "Configuring for Generic ESP32 (2.4 GHz)");
    #else
        ESP_LOGI(TAG, "Configuring for unknown platform");
    #endif
    
    // Validate channel selection
    #if defined(WIFI_CHANNEL)
        #if ESP_PLATFORM == ESP32_C3 || ESP_PLATFORM == ESP32 || ESP_PLATFORM == ESP32_GENERIC
            // 2.4 GHz platforms: channels 1-14
            if (WIFI_CHANNEL < 1 || WIFI_CHANNEL > 14) {
                ESP_LOGW(TAG, "Invalid 2.4 GHz channel %d, using channel 6", WIFI_CHANNEL);
                #undef WIFI_CHANNEL
                #define WIFI_CHANNEL 6
            }
        #elif ESP_PLATFORM == ESP32_C5
            // 5 GHz platform: channels 36-165
            if (WIFI_CHANNEL < 36 || WIFI_CHANNEL > 165) {
                ESP_LOGW(TAG, "Invalid 5 GHz channel %d, using channel 36", WIFI_CHANNEL);
                #undef WIFI_CHANNEL
                #define WIFI_CHANNEL 36
            }
        #endif
    #endif
    
    ESP_LOGI(TAG, "Using WiFi channel: %d", WIFI_CHANNEL);
}

// ============================================================================
// Initialization Functions
// ============================================================================

/**
 * Initialize WiFi for mesh networking
 */
void init_wifi(void) {
    ESP_LOGI(TAG, "Initializing WiFi");
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create WiFi station netif
    esp_netif_create_default_wifi_mesh_netifs(1);
    
    // Initialize WiFi with mesh configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register WiFi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(MESH_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    
    // Configure WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Get MAC address
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, node_mac));
    
    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized, MAC: ");
    print_mac(node_mac);
}

/**
 * Initialize ESP-WiFi-Mesh
 */
void init_mesh(void) {
    ESP_LOGI(TAG, "Initializing ESP-WiFi-Mesh");
    
    // Mesh configuration
    mesh_cfg_t mesh_config = {
        .channel = WIFI_CHANNEL,
        .router.ssid = MESH_ROUTER_SSID,
        .router.password = MESH_ROUTER_PASS,
        #if defined(MESH_ROUTER_BSSID)
        .router.bssid = MESH_ROUTER_BSSID,
        #else
        .router.bssid = {0},
        #endif
        .mesh_id = "glow_mesh",
        .mesh_ap.max_connection = 6,
        .mesh_ap.nonmesh_ap_table_size = 1,
        .mesh_ap.allow_router_switch = true,
        .mesh_ap.allow_channel_switch = true,
        .mesh_ap.max_hop = MESH_MAX_HOPS,
        .mesh_ap.vote_percentage = MESH_VOTE_PERCENT,
        .mesh_ap.backoff_exponent = 3,
        .mesh_ap.max_retries = 3,
        .mesh_ap.beacon_interval = 100, // TUs (1.024ms)
        .mesh_ap.listen_interval = 3,
        .mesh_ap.nonmesh_ap_probe_interval = 30000, // ms
        .mesh_ap.mesh_ap_idle_interval = 10000, // ms
        .mesh_ap.mesh_ap_max_age = 60000, // ms
        .mesh_ap.mesh_ap_fail_interval = 3000, // ms
        .mesh_ap.mesh_ap_select_interval = 1000, // ms
        .mesh_ap.mesh_ap_select_random_interval = 100, // ms
        .mesh_ap.mesh_ap_switch_interval = 1000, // ms
        .mesh_ap.mesh_ap_switch_random_interval = 100, // ms
        .mesh_ap.mesh_ap_beacon_interval = 100, // TUs
        .mesh_ap.mesh_ap_listen_interval = 3,
        .mesh_ap.mesh_ap_scan_interval = 30000, // ms
        .mesh_ap.mesh_ap_idle_interval = 10000, // ms
        .mesh_ap.mesh_ap_max_age = 60000, // ms
    };
    
    // Set mesh configuration
    ESP_ERROR_CHECK(esp_mesh_set_config(&mesh_config));
    
    // Set mesh event callback
    ESP_ERROR_CHECK(esp_mesh_register_tx_cb(mesh_event_handler));
    ESP_ERROR_CHECK(esp_mesh_register_rx_cb(mesh_event_handler));
    
    // Start mesh
    ESP_ERROR_CHECK(esp_mesh_start());
    
    // Wait for mesh to initialize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Check mesh status
    mesh_status_t status;
    ESP_ERROR_CHECK(esp_mesh_get_status(&status));
    
    if (status.mesh_state == MESH_STATE_ROOT) {
        ESP_LOGI(TAG, "Mesh node is ROOT");
    } else if (status.mesh_state == MESH_STATE_IDLE) {
        ESP_LOGI(TAG, "Mesh node is IDLE");
    } else {
        ESP_LOGI(TAG, "Mesh node is ACTIVE");
    }
    
    mesh_initialized = true;
    ESP_LOGI(TAG, "ESP-WiFi-Mesh initialized");
}

/**
 * Initialize UDP socket
 */
void init_udp(void) {
    ESP_LOGI(TAG, "Initializing UDP socket");
    
    // Create UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: %d", errno);
        return;
    }
    
    // Set socket options for broadcast
    int broadcast = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        ESP_LOGE(TAG, "Failed to set broadcast option: %d", errno);
    }
    
    // Bind socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(udp_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind UDP socket: %d", errno);
        close(udp_socket);
        udp_socket = -1;
        return;
    }
    
    ESP_LOGI(TAG, "UDP socket initialized on port %d", UDP_PORT);
}

// ============================================================================
// Neighbor Management Functions
// ============================================================================

/**
 * Initialize neighbor list
 */
void init_neighbor_list(void) {
    memset(neighbor_list, 0, sizeof(neighbor_list));
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        neighbor_list[i].active = false;
        neighbor_list[i].rssi = -127; // Invalid RSSI
        neighbor_list[i].last_seen = 0;
    }
}

/**
 * Update neighbor list with new RSSI reading
 */
void update_neighbor_list(uint8_t *mac, int8_t rssi) {
    if (mac == NULL || memcmp(mac, node_mac, 6) == 0) {
        return; // Ignore null MAC or our own MAC
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000; // ms
    
    // Check if neighbor already exists
    int existing_index = -1;
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && memcmp(neighbor_list[i].mac, mac, 6) == 0) {
            existing_index = i;
            break;
        }
    }
    
    if (existing_index >= 0) {
        // Update existing neighbor
        int8_t rssi_diff = abs(rssi - neighbor_list[existing_index].rssi);
        
        // Only update if RSSI changed significantly or it's been a while
        if (rssi_diff >= RSSI_THRESHOLD || 
            (current_time - neighbor_list[existing_index].last_seen) > 1000) {
            neighbor_list[existing_index].rssi = rssi;
            neighbor_list[existing_index].last_seen = current_time;
            
            if (DEBUG_NEIGHBORS) {
                ESP_LOGD(TAG, "Updated neighbor: ");
                print_mac(mac);
                ESP_LOGD(TAG, " RSSI: %d -> %d", neighbor_list[existing_index].rssi, rssi);
            }
        }
    } else {
        // Find an inactive slot or the weakest neighbor
        int replace_index = -1;
        int8_t weakest_rssi = -30; // Start with a strong signal
        
        for (int i = 0; i < MAX_NEIGHBORS; i++) {
            if (!neighbor_list[i].active) {
                replace_index = i;
                break;
            }
            
            // Track weakest neighbor for potential replacement
            if (neighbor_list[i].rssi < weakest_rssi) {
                weakest_rssi = neighbor_list[i].rssi;
                replace_index = i;
            }
        }
        
        // Only add if we have space or if this RSSI is better than the weakest
        if (replace_index >= 0 && (rssi > weakest_rssi || !neighbor_list[replace_index].active)) {
            memcpy(neighbor_list[replace_index].mac, mac, 6);
            neighbor_list[replace_index].rssi = rssi;
            neighbor_list[replace_index].last_seen = current_time;
            neighbor_list[replace_index].active = true;
            
            // Resolve IP address
            mac_to_ip(mac, neighbor_list[replace_index].ip);
            
            if (DEBUG_NEIGHBORS) {
                ESP_LOGD(TAG, "Added new neighbor: ");
                print_mac(mac);
                ESP_LOGD(TAG, " RSSI: %d", rssi);
            }
        }
    }
    
    // Sort neighbors by RSSI (strongest first)
    for (int i = 0; i < MAX_NEIGHBORS - 1; i++) {
        for (int j = i + 1; j < MAX_NEIGHBORS; j++) {
            if (neighbor_list[i].active && neighbor_list[j].active &&
                neighbor_list[i].rssi < neighbor_list[j].rssi) {
                neighbor_info_t temp = neighbor_list[i];
                neighbor_list[i] = neighbor_list[j];
                neighbor_list[j] = temp;
            }
        }
    }
}

/**
 * Get neighbor by MAC address
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
 * Get number of active neighbors
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
 * Cleanup inactive neighbors (not seen for NEIGHBOR_TIMEOUT_MS)
 */
void cleanup_inactive_neighbors(void) {
    uint32_t current_time = esp_timer_get_time() / 1000; // ms
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && 
            (current_time - neighbor_list[i].last_seen) > NEIGHBOR_TIMEOUT_MS) {
            if (DEBUG_NEIGHBORS) {
                ESP_LOGD(TAG, "Removing inactive neighbor: ");
                print_mac(neighbor_list[i].mac);
            }
            neighbor_list[i].active = false;
            neighbor_list[i].rssi = -127;
        }
    }
}

// ============================================================================
// State Management Functions
// ============================================================================

/**
 * Initialize node state
 */
void init_state(void) {
    reset_node_state();
    set_node_state(NODE_STATE_BOOTING);
}

/**
 * Update node state (called periodically)
 */
void update_state(void) {
    update_node_state();
    
    // Send state to neighbors
    send_state_to_neighbors();
    
    // Adjust TX power based on neighbors
    adjust_tx_power();
}

// ============================================================================
// Communication Functions
// ============================================================================

/**
 * Send state update to all neighbors via UDP unicast
 */
void send_state_to_neighbors(void) {
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "UDP socket not initialized");
        return;
    }
    
    // Prepare message
    memset(&current_message, 0, sizeof(mesh_message_t));
    current_message.version = PROTOCOL_VERSION;
    current_message.msg_type = MSG_TYPE_STATE_UPDATE;
    memcpy(current_message.mac, node_mac, 6);
    memcpy(&current_message.state, &node_state, sizeof(node_state_t));
    
    // Copy neighbor list
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active) {
            memcpy(current_message.neighbors[i].mac, neighbor_list[i].mac, 6);
            current_message.neighbors[i].rssi = neighbor_list[i].rssi;
        }
    }
    
    // Calculate checksum
    current_message.checksum = calculate_checksum(&current_message);
    
    // Send to each neighbor
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active) {
            struct sockaddr_in dest_addr;
            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(UDP_PORT);
            dest_addr.sin_addr.s_addr = htonl(
                (neighbor_list[i].ip[0] << 24) |
                (neighbor_list[i].ip[1] << 16) |
                (neighbor_list[i].ip[2] << 8) |
                neighbor_list[i].ip[3]
            );
            
            if (sendto(udp_socket, &current_message, sizeof(mesh_message_t), 0,
                       (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
                ESP_LOGE(TAG, "Failed to send UDP to neighbor %d: %d", i, errno);
            } else if (DEBUG_UDP) {
                ESP_LOGD(TAG, "Sent UDP state to neighbor: ");
                print_mac(neighbor_list[i].mac);
            }
        }
    }
    
    // Also forward to visualization subscriber
    forward_to_visualization(&current_message);
}

/**
 * Forward message to visualization subscriber via mesh
 */
void forward_to_visualization(mesh_message_t *msg) {
    if (!mesh_initialized) {
        return;
    }
    
    // For now, we'll use UDP broadcast to the visualization IP
    // In a real mesh, we would use esp_mesh_send()
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(VISUALIZATION_PORT);
    
    // Parse visualization IP
    uint32_t ip_addr = 0;
    const char *ip_str = VISUALIZATION_IP;
    char ip_copy[16];
    strncpy(ip_copy, ip_str, sizeof(ip_copy) - 1);
    ip_copy[sizeof(ip_copy) - 1] = '\0';
    
    char *token = strtok(ip_copy, ".");
    for (int i = 0; i < 4 && token != NULL; i++) {
        ip_addr = (ip_addr << 8) | atoi(token);
        token = strtok(NULL, ".");
    }
    dest_addr.sin_addr.s_addr = htonl(ip_addr);
    
    // Send via UDP (will be forwarded by mesh if needed)
    if (udp_socket >= 0) {
        if (sendto(udp_socket, msg, sizeof(mesh_message_t), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            ESP_LOGE(TAG, "Failed to forward to visualization: %d", errno);
        } else if (DEBUG_UDP) {
            ESP_LOGD(TAG, "Forwarded to visualization: " VISUALIZATION_IP);
        }
    }
}

/**
 * Send UDP message to specific destination
 */
void send_udp_message(uint8_t *dest_ip, uint16_t dest_port, mesh_message_t *msg) {
    if (udp_socket < 0) return;
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    dest_addr.sin_addr.s_addr = htonl(
        (dest_ip[0] << 24) |
        (dest_ip[1] << 16) |
        (dest_ip[2] << 8) |
        dest_ip[3]
    );
    
    if (sendto(udp_socket, msg, sizeof(mesh_message_t), 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to send UDP message: %d", errno);
    }
}

// ============================================================================
// TX Power Management Functions
// ============================================================================

/**
 * Initialize TX power management
 */
void init_tx_power(void) {
    // Set initial TX power
    esp_wifi_set_max_tx_power(current_tx_power);
    
    if (DEBUG_TX_POWER) {
        ESP_LOGD(TAG, "Initial TX power set to: %d (%.1f dBm)", 
                 current_tx_power, (current_tx_power - 8) / 4.0);
    }
}

/**
 * Adjust TX power based on weakest neighbor RSSI
 */
void adjust_tx_power(void) {
    int8_t weakest_rssi = -30; // Start with strong signal
    bool has_neighbors = false;
    
    // Find weakest neighbor RSSI
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active) {
            has_neighbors = true;
            if (neighbor_list[i].rssi < weakest_rssi) {
                weakest_rssi = neighbor_list[i].rssi;
            }
        }
    }
    
    if (!has_neighbors) {
        // No neighbors, use minimum power
        if (current_tx_power != MIN_TX_POWER) {
            current_tx_power = MIN_TX_POWER;
            esp_wifi_set_max_tx_power(current_tx_power);
            if (DEBUG_TX_POWER) {
                ESP_LOGD(TAG, "No neighbors, TX power set to minimum: %d", current_tx_power);
            }
        }
        return;
    }
    
    // Calculate required TX power
    int8_t required_power = calculate_required_tx_power();
    
    // Adjust TX power towards required power
    if (required_power > current_tx_power) {
        current_tx_power = MIN(current_tx_power + TX_POWER_STEP, MAX_TX_POWER);
    } else if (required_power < current_tx_power) {
        current_tx_power = MAX(current_tx_power - TX_POWER_STEP, MIN_TX_POWER);
    }
    
    // Set new TX power
    if (esp_wifi_set_max_tx_power(current_tx_power) == ESP_OK) {
        if (DEBUG_TX_POWER) {
            ESP_LOGD(TAG, "Adjusted TX power: %d (%.1f dBm), weakest RSSI: %d dBm", 
                     current_tx_power, (current_tx_power - 8) / 4.0, weakest_rssi);
        }
    }
}

/**
 * Calculate required TX power based on weakest neighbor RSSI
 */
int8_t calculate_required_tx_power(void) {
    int8_t weakest_rssi = -30;
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && neighbor_list[i].rssi < weakest_rssi) {
            weakest_rssi = neighbor_list[i].rssi;
        }
    }
    
    // Simple proportional control: adjust TX power to maintain target RSSI
    // If RSSI is too low (more negative), increase TX power
    // If RSSI is too high (less negative), decrease TX power
    
    int8_t rssi_diff = TARGET_RSSI - weakest_rssi;
    
    // Convert RSSI difference to TX power adjustment
    // Each 4 dB of TX power change ≈ 1 dBm RSSI change at receiver
    // So we need to adjust by (rssi_diff / 4) * 4 (since ESP32 TX power is in 4 dB steps)
    int8_t adjustment = (rssi_diff / 4) * 4;
    
    int8_t required_power = current_tx_power + adjustment;
    
    // Clamp to valid range
    return CLAMP(required_power, MIN_TX_POWER, MAX_TX_POWER);
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Calculate simple checksum for message
 */
uint16_t calculate_checksum(mesh_message_t *msg) {
    uint16_t checksum = 0;
    uint8_t *data = (uint8_t *)msg;
    size_t len = sizeof(mesh_message_t) - sizeof(msg->checksum);
    
    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    
    return checksum;
}

/**
 * Verify message checksum
 */
bool verify_checksum(mesh_message_t *msg) {
    uint16_t calculated = calculate_checksum(msg);
    return calculated == msg->checksum;
}

/**
 * Convert MAC address to static IP (192.168.1.<last_byte>)
 */
void mac_to_ip(uint8_t *mac, uint8_t *ip) {
    ip[0] = 192;
    ip[1] = 168;
    ip[2] = 1;
    ip[3] = mac[5]; // Use last byte of MAC
}

/**
 * Print MAC address
 */
void print_mac(uint8_t *mac) {
    if (mac == NULL) {
        ESP_LOGD(TAG, "NULL");
        return;
    }
    ESP_LOGD(TAG, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * Print IP address
 */
void print_ip(uint8_t *ip) {
    if (ip == NULL) {
        ESP_LOGD(TAG, "NULL");
        return;
    }
    ESP_LOGD(TAG, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

// ============================================================================
// Event Handlers
// ============================================================================

/**
 * Mesh event handler
 */
esp_err_t mesh_event_handler(mesh_event_t event) {
    switch (event.id) {
        case MESH_EVENT_STARTED: {
            ESP_LOGI(TAG, "Mesh started");
            mesh_initialized = true;
            set_node_state(NODE_STATE_ACTIVE);
            break;
        }
        case MESH_EVENT_STOPPED: {
            ESP_LOGI(TAG, "Mesh stopped");
            mesh_initialized = false;
            set_node_state(NODE_STATE_IDLE);
            break;
        }
        case MESH_EVENT_CHILD_CONNECTED: {
            mesh_event_child_connected_t *child = (mesh_event_child_connected_t *)event.data;
            ESP_LOGI(TAG, "Child connected: ");
            print_mac(child->mac);
            break;
        }
        case MESH_EVENT_CHILD_DISCONNECTED: {
            mesh_event_child_disconnected_t *child = (mesh_event_child_disconnected_t *)event.data;
            ESP_LOGI(TAG, "Child disconnected: ");
            print_mac(child->mac);
            break;
        }
        case MESH_EVENT_ROUTING_TABLE_ADD: {
            mesh_event_routing_table_change_t *routing = (mesh_event_routing_table_change_t *)event.data;
            ESP_LOGI(TAG, "Routing table add: ");
            print_mac(routing->mac);
            break;
        }
        case MESH_EVENT_ROUTING_TABLE_REMOVE: {
            mesh_event_routing_table_change_t *routing = (mesh_event_routing_table_change_t *)event.data;
            ESP_LOGI(TAG, "Routing table remove: ");
            print_mac(routing->mac);
            break;
        }
        case MESH_EVENT_NO_PARENT_FOUND: {
            mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event.data;
            ESP_LOGW(TAG, "No parent found, scan count: %d", no_parent->scan_count);
            break;
        }
        case MESH_EVENT_PARENT_CONNECTED: {
            mesh_event_connected_t *connected = (mesh_event_connected_t *)event.data;
            ESP_LOGI(TAG, "Parent connected: ");
            print_mac(connected->mac);
            break;
        }
        case MESH_EVENT_PARENT_DISCONNECTED: {
            mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event.data;
            ESP_LOGI(TAG, "Parent disconnected: ");
            print_mac(disconnected->mac);
            break;
        }
        case MESH_EVENT_LAYER_CHANGE: {
            mesh_event_layer_change_t *layer = (mesh_event_layer_change_t *)event.data;
            ESP_LOGI(TAG, "Layer change: %d", layer->new_layer);
            break;
        }
        case MESH_EVENT_ROOT_ADDRESS: {
            mesh_event_root_address_t *root = (mesh_event_root_address_t *)event.data;
            ESP_LOGI(TAG, "Root address: ");
            print_mac(root->addr);
            break;
        }
        case MESH_EVENT_VOTE_STARTED: {
            mesh_event_vote_started_t *vote = (mesh_event_vote_started_t *)event.data;
            ESP_LOGI(TAG, "Vote started, attempts: %d", vote->attempts);
            break;
        }
        case MESH_EVENT_VOTE_STOPPED: {
            ESP_LOGI(TAG, "Vote stopped");
            break;
        }
        case MESH_EVENT_ROOT_SWITCH_REQ: {
            mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event.data;
            ESP_LOGI(TAG, "Root switch request: ");
            print_mac(switch_req->candidate_mac);
            break;
        }
        case MESH_EVENT_ROOT_SWITCH_ACK: {
            mesh_event_root_switch_ack_t *switch_ack = (mesh_event_root_switch_ack_t *)event.data;
            ESP_LOGI(TAG, "Root switch ack: ");
            print_mac(switch_ack->new_root_mac);
            break;
        }
        case MESH_EVENT_TODS_STATE: {
            mesh_event_toDS_state_t *toDS = (mesh_event_toDS_state_t *)event.data;
            ESP_LOGI(TAG, "ToDS state: %d", toDS->toDS);
            break;
        }
        case MESH_EVENT_ROOT_FIXED: {
            mesh_event_root_fixed_t *fixed = (mesh_event_root_fixed_t *)event.data;
            ESP_LOGI(TAG, "Root fixed: %d", fixed->is_fixed);
            break;
        }
        case MESH_EVENT_ROOT_ASKED_YIELD: {
            ESP_LOGI(TAG, "Root asked to yield");
            break;
        }
        case MESH_EVENT_CHANNEL_SWITCH: {
            mesh_event_channel_switch_t *channel = (mesh_event_channel_switch_t *)event.data;
            ESP_LOGI(TAG, "Channel switch to: %d", channel->new_channel);
            break;
        }
        case MESH_EVENT_SCAN_DONE: {
            mesh_event_scan_done_t *scan = (mesh_event_scan_done_t *)event.data;
            ESP_LOGI(TAG, "Scan done, number: %d", scan->number);
            break;
        }
        case MESH_EVENT_NETWORK_STATE: {
            mesh_event_network_state_t *network = (mesh_event_network_state_t *)event.data;
            ESP_LOGI(TAG, "Network state: %d", network->is_connected);
            break;
        }
        case MESH_EVENT_STOP_RECONNECTION: {
            ESP_LOGI(TAG, "Stop reconnection");
            break;
        }
        case MESH_EVENT_FIND_NETWORK: {
            mesh_event_find_network_t *find = (mesh_event_find_network_t *)event.data;
            ESP_LOGI(TAG, "Find network: %d", find->new_network);
            break;
        }
        case MESH_EVENT_ROUTER_SWITCH: {
            mesh_event_router_switch_t *router = (mesh_event_router_switch_t *)event.data;
            ESP_LOGI(TAG, "Router switch: ");
            print_mac(router->new_router.mac);
            break;
        }
        default:
            ESP_LOGD(TAG, "Unhandled mesh event: %d", event.id);
            break;
    }
    
    return ESP_OK;
}

/**
 * WiFi event handler
 */
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_WIFI_READY:
                ESP_LOGI(TAG, "WiFi ready");
                break;
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "WiFi scan done");
                break;
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA start");
                break;
            case WIFI_EVENT_STA_STOP:
                ESP_LOGI(TAG, "STA stop");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "STA connected");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "STA disconnected");
                break;
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                ESP_LOGI(TAG, "STA auth mode change");
                break;
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP start");
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "AP stop");
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "AP STA connected");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "AP STA disconnected");
                break;
            default:
                ESP_LOGD(TAG, "Unhandled WiFi event: %d", event_id);
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "Got IP address");
                break;
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI(TAG, "Lost IP address");
                break;
            case IP_EVENT_AP_STAIPASSIGNED:
                ESP_LOGI(TAG, "AP assigned IP to STA");
                break;
            default:
                ESP_LOGD(TAG, "Unhandled IP event: %d", event_id);
                break;
        }
    } else if (event_base == MESH_EVENT) {
        mesh_event_t event;
        event.id = event_id;
        event.data = event_data;
        mesh_event_handler(event);
    }
}

// ============================================================================
// Timer Callbacks
// ============================================================================

/**
 * State update timer callback
 */
void state_update_timer_callback(void *arg) {
    update_state();
}

/**
 * Neighbor cleanup timer callback
 */
void neighbor_cleanup_timer_callback(void *arg) {
    cleanup_inactive_neighbors();
}

// ============================================================================
// Initialization Function (called from main)
// ============================================================================

/**
 * Initialize all mesh node components
 */
void mesh_node_init(void) {
    ESP_LOGI(TAG, "Initializing mesh node");
    
    // Initialize neighbor list
    init_neighbor_list();
    
    // Initialize WiFi
    init_wifi();
    
    // Initialize mesh
    init_mesh();
    
    // Initialize UDP
    init_udp();
    
    // Initialize beacon monitoring
    init_beacon_monitoring();
    
    // Initialize state manager
    init_state_manager();
    
    // Initialize TX power
    init_tx_power();
    
    // Initialize state
    init_state();
    
    // Create timers
    esp_timer_create_args_t state_timer_args = {
        .callback = state_update_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "state_update"
    };
    esp_timer_create(&state_timer_args, &state_update_timer);
    
    esp_timer_create_args_t cleanup_timer_args = {
        .callback = neighbor_cleanup_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "neighbor_cleanup"
    };
    esp_timer_create(&cleanup_timer_args, &neighbor_cleanup_timer);
    
    // Start timers
    esp_timer_start_periodic(state_update_timer, STATE_UPDATE_INTERVAL * 1000); // Convert to microseconds
    esp_timer_start_periodic(neighbor_cleanup_timer, NEIGHBOR_TIMEOUT_MS * 1000);
    
    ESP_LOGI(TAG, "Mesh node initialized successfully");
}
