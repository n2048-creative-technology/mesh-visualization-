/**
 * Mesh Node Implementation
 * Core mesh network functionality for ESP32
 * PlatformIO compatible for VS Code
 * Supports: ESP32-C3 (2.4 GHz), ESP32-C5 (5 GHz), Generic ESP32
 */

#include "mesh_node.h"
#include "beacon_monitor.h"
#include "state_manager.h"
#include "esp32_arduino_compat.h"
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

// Timer handles (simplified for Arduino)
void* state_update_timer = NULL;
void* neighbor_cleanup_timer = NULL;

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
    
    // Initialize sensors and LEDs
    init_state();
}

// ============================================================================
// Initialization Functions
// ============================================================================

/**
 * Initialize WiFi for mesh networking
 */
void init_wifi(void) {
    ESP_LOGI(TAG, "Initializing WiFi");
    
    #if defined(ARDUINO)
    // Arduino framework
    WiFi.mode(WIFI_STA);
    
    // Get MAC address
    uint8_t* arduino_mac = WiFi.macAddress();
    for (int i = 0; i < 6; i++) {
        node_mac[i] = arduino_mac[i];
    }
    
    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized, MAC: ");
    print_mac(node_mac);
    
    #else
    // ESP-IDF framework
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create WiFi station netif
    esp_netif_create_default_wifi_mesh_netifs(1);
    
    // Initialize WiFi with mesh configuration
    void* cfg = NULL;
    ESP_ERROR_CHECK(esp_wifi_init(cfg));
    
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
    #endif
}

/**
 * Initialize ESP-WiFi-Mesh (or simulated mesh for Arduino)
 */
void init_mesh(void) {
    ESP_LOGI(TAG, "Initializing mesh network");
    
    #if defined(ARDUINO)
    // Arduino framework - simulate mesh initialization
    // In Arduino, we'll use WiFi in STA mode and broadcast UDP messages
    
    // Initialize WiFi
    init_wifi();
    
    // Connect to WiFi network (for testing)
    WiFi.begin(MESH_ROUTER_SSID, MESH_ROUTER_PASS);
    
    // Wait for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        ESP_LOGI(TAG, "Connecting to WiFi... attempt %d", attempts);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI(TAG, "Connected to WiFi network");
        ESP_LOGI(TAG, "IP address: %s", WiFi.localIP().toString().c_str());
    } else {
        ESP_LOGW(TAG, "Failed to connect to WiFi, using AP mode");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(MESH_ROUTER_SSID, MESH_ROUTER_PASS);
        ESP_LOGI(TAG, "AP mode started, IP: %s", WiFi.softAPIP().toString().c_str());
    }
    
    // Initialize neighbor list
    init_neighbor_list();
    
    mesh_initialized = true;
    ESP_LOGI(TAG, "Mesh network initialized (Arduino simulation)");
    
    #else
    // ESP-IDF framework
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
    
    // Initialize neighbor list
    init_neighbor_list();
    
    mesh_initialized = true;
    ESP_LOGI(TAG, "ESP-WiFi-Mesh initialized");
    #endif
}

/**
 * Initialize UDP socket
 */
void init_udp(void) {
    ESP_LOGI(TAG, "Initializing UDP socket");
    
    #if defined(ARDUINO)
    // Arduino framework - use our wrapper
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: %d", errno);
        return;
    }
    
    if (bind(udp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind UDP socket: %d", errno);
        close(udp_socket);
        udp_socket = -1;
        return;
    }
    
    // Set socket to non-blocking
    int flags = fcntl(udp_socket, F_GETFL, 0);
    fcntl(udp_socket, F_SETFL, flags | O_NONBLOCK);
    
    ESP_LOGI(TAG, "UDP socket initialized on port %d", UDP_PORT);
    
    #else
    // ESP-IDF framework
    // Create UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: %d", errno);
        return;
    }
    
    // Bind socket
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
    
    // Set socket to non-blocking
    int flags = fcntl(udp_socket, F_GETFL, 0);
    fcntl(udp_socket, F_SETFL, flags | O_NONBLOCK);
    
    ESP_LOGI(TAG, "UDP socket initialized on port %d", UDP_PORT);
    #endif
}

// ============================================================================
// Neighbor Management
// ============================================================================

/**
 * Initialize neighbor list
 */
void init_neighbor_list(void) {
    ESP_LOGI(TAG, "Initializing neighbor list");
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        neighbor_list[i].active = false;
        neighbor_list[i].rssi = -127; // Invalid RSSI
        neighbor_list[i].last_seen = 0;
        memset(neighbor_list[i].mac, 0, 6);
        memset(neighbor_list[i].ip, 0, 4);
    }
}

/**
 * Update neighbor list with new information
 */
void update_neighbor_list(uint8_t *mac, int8_t rssi) {
    if (!mac) return;
    
    // Check if neighbor already exists
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && memcmp(neighbor_list[i].mac, mac, 6) == 0) {
            // Update existing neighbor
            neighbor_list[i].rssi = rssi;
            neighbor_list[i].last_seen = millis();
            
            if (DEBUG_NEIGHBORS) {
                ESP_LOGD(TAG, "Updated neighbor: ");
                print_mac(mac);
                ESP_LOGD(TAG, " RSSI: %d dBm", rssi);
            }
            return;
        }
    }
    
    // Find an empty slot
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (!neighbor_list[i].active) {
            // Add new neighbor
            memcpy(neighbor_list[i].mac, mac, 6);
            neighbor_list[i].rssi = rssi;
            neighbor_list[i].last_seen = millis();
            neighbor_list[i].active = true;
            
            // Generate IP from MAC
            mac_to_ip(mac, neighbor_list[i].ip);
            
            if (DEBUG_NEIGHBORS) {
                ESP_LOGD(TAG, "Added new neighbor: ");
                print_mac(mac);
                ESP_LOGD(TAG, " RSSI: %d dBm", rssi);
            }
            return;
        }
    }
    
    // No space for new neighbor
    ESP_LOGW(TAG, "Neighbor list full, cannot add new neighbor");
}

/**
 * Cleanup inactive neighbors
 */
void cleanup_inactive_neighbors(void) {
    uint32_t current_time = millis();
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && 
            (current_time - neighbor_list[i].last_seen) > NEIGHBOR_TIMEOUT_MS) {
            neighbor_list[i].active = false;
            
            if (DEBUG_NEIGHBORS) {
                ESP_LOGD(TAG, "Removed inactive neighbor: ");
                print_mac(neighbor_list[i].mac);
            }
        }
    }
}

/**
 * Get neighbor by MAC address
 */
neighbor_info_t* get_neighbor_by_mac(uint8_t *mac) {
    if (!mac) return NULL;
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && memcmp(neighbor_list[i].mac, mac, 6) == 0) {
            return &neighbor_list[i];
        }
    }
    
    return NULL;
}

/**
 * Get neighbor count
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

// ============================================================================
// State Management
// ============================================================================

/**
 * Set node state
 */
void set_node_state(uint8_t state) {
    node_state.state = state;
    node_state.timestamp = millis() / 1000; // Convert to seconds
    
    ESP_LOGI(TAG, "Node state set to: %d", state);
}

// ============================================================================
// Communication
// ============================================================================

/**
 * Send state to neighbors
 */
void send_state_to_neighbors(void) {
    if (!mesh_initialized) return;
    
    // Build message
    current_message.version = PROTOCOL_VERSION;
    current_message.msg_type = MSG_TYPE_STATE_UPDATE;
    memcpy(current_message.mac, node_mac, 6);
    memcpy(&current_message.state, &node_state, sizeof(node_state_t));
    
    // Add neighbors to message
    int neighbor_count = get_neighbor_count();
    for (int i = 0; i < MIN(neighbor_count, MAX_NEIGHBORS); i++) {
        if (neighbor_list[i].active) {
            memcpy(&current_message.neighbors[i], &neighbor_list[i], sizeof(neighbor_info_t));
        }
    }
    
    // Calculate checksum
    current_message.checksum = calculate_checksum(&current_message);
    
    // Send to all neighbors
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active) {
            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(UDP_PORT);
            memcpy(&dest.sin_addr.s_addr, neighbor_list[i].ip, 4);
            
            sendto(udp_socket, &current_message, sizeof(current_message), 0,
                   (struct sockaddr*)&dest, sizeof(dest));
        }
    }
    
    if (DEBUG_UDP) {
        ESP_LOGD(TAG, "Sent state update to %d neighbors", neighbor_count);
    }
}

/**
 * Forward message to visualization server
 */
void forward_to_visualization(mesh_message_t *msg) {
    if (!msg) return;
    
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(VISUALIZATION_PORT);
    
    // Parse VISUALIZATION_IP
    IPAddress viz_ip;
    if (viz_ip.fromString(VISUALIZATION_IP)) {
        dest.sin_addr.s_addr = viz_ip;
    } else {
        ESP_LOGW(TAG, "Invalid visualization IP: %s", VISUALIZATION_IP);
        return;
    }
    
    sendto(udp_socket, msg, sizeof(mesh_message_t), 0,
           (struct sockaddr*)&dest, sizeof(dest));
    
    if (DEBUG_UDP) {
        ESP_LOGD(TAG, "Forwarded message to visualization server");
    }
}

/**
 * Broadcast state to all nodes
 */
void broadcast_state(void) {
    if (!mesh_initialized) return;
    
    // Build message
    current_message.version = PROTOCOL_VERSION;
    current_message.msg_type = MSG_TYPE_STATE_UPDATE;
    memcpy(current_message.mac, node_mac, 6);
    memcpy(&current_message.state, &node_state, sizeof(node_state_t));
    
    // Add neighbors to message
    int neighbor_count = get_neighbor_count();
    for (int i = 0; i < MIN(neighbor_count, MAX_NEIGHBORS); i++) {
        if (neighbor_list[i].active) {
            memcpy(&current_message.neighbors[i], &neighbor_list[i], sizeof(neighbor_info_t));
        }
    }
    
    // Calculate checksum
    current_message.checksum = calculate_checksum(&current_message);
    
    // Broadcast to all nodes
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(UDP_PORT);
    dest.sin_addr.s_addr = INADDR_BROADCAST;
    
    sendto(udp_socket, &current_message, sizeof(current_message), 0,
           (struct sockaddr*)&dest, sizeof(dest));
    
    if (DEBUG_UDP) {
        ESP_LOGD(TAG, "Broadcasted state to all nodes");
    }
}

/**
 * Send UDP message to specific destination
 */
void send_udp_message(uint8_t *dest_ip, uint16_t dest_port, mesh_message_t *msg) {
    if (!msg || !dest_ip) return;
    
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(dest_port);
    memcpy(&dest.sin_addr.s_addr, dest_ip, 4);
    
    sendto(udp_socket, msg, sizeof(mesh_message_t), 0,
           (struct sockaddr*)&dest, sizeof(dest));
}

// ============================================================================
// TX Power Management
// ============================================================================

/**
 * Initialize TX power
 */
void init_tx_power(void) {
    ESP_LOGI(TAG, "Initializing TX power management");
    
    #if defined(ARDUINO)
    // Arduino framework - set initial TX power
    // Note: ESP32 Arduino doesn't have direct TX power control
    // This is a limitation of the Arduino framework
    current_tx_power = 8; // Default: 0 dBm
    
    #else
    // ESP-IDF framework
    esp_wifi_set_max_tx_power(MAX_TX_POWER);
    current_tx_power = MIN_TX_POWER;
    esp_wifi_set_tx_power(current_tx_power);
    
    #endif
    
    ESP_LOGI(TAG, "Initial TX power: %d dBm", current_tx_power);
}

/**
 * Adjust TX power based on neighbor RSSI
 */
void adjust_tx_power(void) {
    if (!mesh_initialized) return;
    
    int8_t required_power = calculate_required_tx_power();
    
    if (required_power != current_tx_power) {
        current_tx_power = required_power;
        
        #if !defined(ARDUINO)
        // ESP-IDF framework - set TX power
        esp_wifi_set_tx_power(current_tx_power);
        #endif
        
        if (DEBUG_TX_POWER) {
            ESP_LOGD(TAG, "Adjusted TX power to: %d dBm", current_tx_power);
        }
    }
}

/**
 * Calculate required TX power based on weakest neighbor
 */
int8_t calculate_required_tx_power(void) {
    int8_t weakest_rssi = -127; // Start with worst possible RSSI
    
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_list[i].active && neighbor_list[i].rssi < weakest_rssi) {
            weakest_rssi = neighbor_list[i].rssi;
        }
    }
    
    if (weakest_rssi == -127) {
        // No neighbors, use minimum power
        return MIN_TX_POWER;
    }
    
    // Calculate required power to achieve target RSSI
    int8_t required_power = current_tx_power + (TARGET_RSSI - weakest_rssi);
    
    // Clamp to valid range
    return CLAMP(required_power, MIN_TX_POWER, MAX_TX_POWER);
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Calculate checksum for a message
 */
uint16_t calculate_checksum(mesh_message_t *msg) {
    if (!msg) return 0;
    
    uint16_t checksum = 0;
    uint8_t *data = (uint8_t*)msg;
    
    // Calculate checksum over all bytes except the checksum field itself
    size_t length = sizeof(mesh_message_t) - sizeof(msg->checksum);
    
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    
    return checksum;
}

/**
 * Verify checksum of a message
 */
bool verify_checksum(mesh_message_t *msg) {
    if (!msg) return false;
    
    uint16_t calculated = calculate_checksum(msg);
    return (calculated == msg->checksum);
}

/**
 * Convert MAC address to IP address
 */
void mac_to_ip(uint8_t *mac, uint8_t *ip) {
    if (!mac || !ip) return;
    
    // Simple mapping: 192.168.1.<last_byte_of_mac>
    ip[0] = 192;
    ip[1] = 168;
    ip[2] = 1;
    ip[3] = mac[5]; // Use last byte of MAC
}

/**
 * Print MAC address
 */
void print_mac(uint8_t *mac) {
    if (!mac) return;
    
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * Print IP address
 */
void print_ip(uint8_t *ip) {
    if (!ip) return;
    
    Serial.printf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

// ============================================================================
// Event Handlers
// ============================================================================

/**
 * Mesh event handler
 */
#if defined(ARDUINO)
void mesh_event_handler(mesh_event_t event) {
    // Arduino framework - simplified event handling
    switch(event.id) {
        case MESH_EVENT_ROOT_GOT_IP:
            ESP_LOGI(TAG, "Mesh root got IP");
            break;
        case MESH_EVENT_ROOT_LOST_IP:
            ESP_LOGW(TAG, "Mesh root lost IP");
            break;
        case MESH_EVENT_NO_PARENT:
            ESP_LOGW(TAG, "No parent found");
            break;
        case MESH_EVENT_PARENT_CONNECTED:
            ESP_LOGI(TAG, "Parent connected");
            break;
        case MESH_EVENT_PARENT_DISCONNECTED:
            ESP_LOGW(TAG, "Parent disconnected");
            break;
        case MESH_EVENT_LAYER_CHANGE:
            ESP_LOGI(TAG, "Layer changed");
            break;
        case MESH_EVENT_CHILD_CONNECTED:
            ESP_LOGI(TAG, "Child connected");
            break;
        case MESH_EVENT_CHILD_DISCONNECTED:
            ESP_LOGI(TAG, "Child disconnected");
            break;
        default:
            ESP_LOGD(TAG, "Unhandled mesh event: %d", event.id);
            break;
    }
}
#else
esp_err_t mesh_event_handler(mesh_event_t event) {
    // ESP-IDF framework - full event handling
    switch(event.id) {
        case MESH_EVENT_ROOT_GOT_IP:
            ESP_LOGI(TAG, "Mesh root got IP");
            break;
        case MESH_EVENT_ROOT_LOST_IP:
            ESP_LOGW(TAG, "Mesh root lost IP");
            break;
        case MESH_EVENT_NO_PARENT:
            ESP_LOGW(TAG, "No parent found");
            break;
        case MESH_EVENT_PARENT_CONNECTED:
            ESP_LOGI(TAG, "Parent connected");
            break;
        case MESH_EVENT_PARENT_DISCONNECTED:
            ESP_LOGW(TAG, "Parent disconnected");
            break;
        case MESH_EVENT_LAYER_CHANGE:
            ESP_LOGI(TAG, "Layer changed");
            break;
        case MESH_EVENT_CHILD_CONNECTED:
            ESP_LOGI(TAG, "Child connected");
            break;
        case MESH_EVENT_CHILD_DISCONNECTED:
            ESP_LOGI(TAG, "Child disconnected");
            break;
        default:
            ESP_LOGD(TAG, "Unhandled mesh event: %d", event.id);
            break;
    }
    
    return ESP_OK;
}
#endif

/**
 * WiFi event handler
 */
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    #if defined(ARDUINO)
    // Arduino framework - simplified event handling
    if (event_base == WIFI_EVENT) {
        switch(event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi disconnected");
                break;
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WiFi AP started");
                break;
            default:
                ESP_LOGD(TAG, "Unhandled WiFi event: %d", event_id);
                break;
        }
    }
    #else
    // ESP-IDF framework - full event handling
    if (event_base == WIFI_EVENT) {
        switch(event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi disconnected");
                break;
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WiFi AP started");
                break;
            default:
                ESP_LOGD(TAG, "Unhandled WiFi event: %d", event_id);
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch(event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "Got IP address");
                break;
            default:
                ESP_LOGD(TAG, "Unhandled IP event: %d", event_id);
                break;
        }
    } else if (event_base == MESH_EVENT) {
        mesh_event_handler(*(mesh_event_t*)event_data);
    }
    #endif
}
