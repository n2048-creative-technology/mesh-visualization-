/**
 * Main Entry Point for ESP32 Mesh Node
 * ESP-IDF Framework with ESP-WiFi-Mesh
 * Seeed Studio XIAO ESP32-C3
 */

#include <string.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "fcntl.h"

#include "config.h"
#include "mesh_node.h"
#include "state_manager.h"
#include "beacon_monitor.h"
#if ENABLE_MQTT_VISUALIZATION
#include "mqtt_handler.h"
#endif

static const char *TAG = "MAIN";
static esp_netif_t *mesh_netif_sta = NULL;
static uint32_t last_mesh_attached_ms = 0;
static uint32_t last_mesh_health_check_ms = 0;

// Global MAC address
uint8_t node_mac[6] = {0};

// TCP Server
int tcp_server_socket = -1;

// Forward declarations
void start_tcp_server(void);
void send_state_update_to_visualization(void);
bool has_ip_address(void);
static bool state_publish_relevant_change(const node_state_t *previous, const node_state_t *current);
static void init_nvs_storage(void);
static bool mac_is_zero(const uint8_t *mac);
static void check_mesh_health(uint32_t now);

/**
 * IP event handler
 */
void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));
        
        // Now that we have IP, start UDP and TCP services
        init_udp();
        start_tcp_server();
#if ENABLE_MQTT_VISUALIZATION
        init_mqtt();
#endif
    }
}

/**
 * Mesh event handler
 */
void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    switch (event_id) {
        case MESH_EVENT_STARTED: {
            ESP_LOGI(TAG, "<MESH_EVENT_STARTED>");
            break;
        }
        case MESH_EVENT_STOPPED: {
            ESP_LOGI(TAG, "<MESH_EVENT_STOPPED>");
            mesh_initialized = false;
            has_current_root_addr = false;
            break;
        }
        case MESH_EVENT_CHILD_CONNECTED: {
            mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, " MACSTR "", 
                     child_connected->aid, MAC2STR(child_connected->mac));
            break;
        }
        case MESH_EVENT_CHILD_DISCONNECTED: {
            mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, " MACSTR "", 
                     child_disconnected->aid, MAC2STR(child_disconnected->mac));
            break;
        }
        case MESH_EVENT_PARENT_CONNECTED: {
            mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_PARENT_CONNECTED>layer:%d", connected->self_layer);
            last_mesh_attached_ms = esp_timer_get_time() / 1000;
            if (esp_mesh_is_root()) {
                memcpy(current_root_addr.addr, node_mac, sizeof(current_root_addr.addr));
                has_current_root_addr = true;
                ESP_LOGI(TAG, "Node is ROOT - starting UDP and TCP services");
                if (mesh_netif_sta != NULL) {
                    esp_err_t err = esp_netif_dhcpc_start(mesh_netif_sta);
                    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
                        ESP_LOGW(TAG, "Failed to start root DHCP client: %s", esp_err_to_name(err));
                    }
                }
                init_udp();
                start_tcp_server();
#if ENABLE_MQTT_VISUALIZATION
                if (has_ip_address()) {
                    init_mqtt();
                }
#endif
            }
            break;
        }
        case MESH_EVENT_ROOT_ADDRESS: {
            mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
            memcpy(current_root_addr.addr, root_addr->addr, sizeof(current_root_addr.addr));
            if (!esp_mesh_is_root() && current_root_addr.addr[5] > 0) {
                current_root_addr.addr[5]--;
            }
            has_current_root_addr = true;
            ESP_LOGI(TAG, "<MESH_EVENT_ROOT_ADDRESS>" MACSTR, MAC2STR(current_root_addr.addr));
            break;
        }
        case MESH_EVENT_PARENT_DISCONNECTED: {
            mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d", disconnected->reason);
            has_current_root_addr = false;
            break;
        }
        case MESH_EVENT_LAYER_CHANGE: {
            mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_LAYER_CHANGE>new_layer:%d", layer_change->new_layer);
            if (layer_change->new_layer > 0) {
                last_mesh_attached_ms = esp_timer_get_time() / 1000;
            }
            break;
        }
        case MESH_EVENT_NO_PARENT_FOUND: {
            mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d", no_parent->scan_times);
            break;
        }
        case MESH_EVENT_TODS_STATE: {
            mesh_event_toDS_state_t *toDS_state = (mesh_event_toDS_state_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_TODS_STATE>state:%d", *toDS_state);
            break;
        }
        default:
            ESP_LOGD(TAG, "Unhandled mesh event: %d", event_id);
            break;
    }
}

/**
 * TCP Server for receiving commands from visualization tool
 */
#define TCP_BACKLOG 5

void start_tcp_server(void) {
    if (tcp_server_socket >= 0) {
        ESP_LOGI(TAG, "TCP server already running");
        return;
    }
    
    ESP_LOGI(TAG, "Starting TCP server on port %d", TCP_PORT);
    
    tcp_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_server_socket < 0) {
        ESP_LOGE(TAG, "Failed to create TCP socket: %d", errno);
        return;
    }
    
    int opt = 1;
    if (setsockopt(tcp_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: %d", errno);
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(tcp_server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind TCP socket: %d", errno);
        close(tcp_server_socket);
        tcp_server_socket = -1;
        return;
    }
    
    if (listen(tcp_server_socket, TCP_BACKLOG) < 0) {
        ESP_LOGE(TAG, "Failed to listen on TCP socket: %d", errno);
        close(tcp_server_socket);
        tcp_server_socket = -1;
        return;
    }

    int flags = fcntl(tcp_server_socket, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(tcp_server_socket, F_SETFL, flags | O_NONBLOCK);
    }
    
    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);
}

void handle_tcp_client(int client_socket) {
    char buffer[512];
    ssize_t bytes_read;
    
    while (1) {
        bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                ESP_LOGI(TAG, "TCP client disconnected");
            } else {
                ESP_LOGE(TAG, "TCP receive error: %d", errno);
            }
            break;
        }
        
        buffer[bytes_read] = '\0';
        ESP_LOGI(TAG, "TCP command received: %s", buffer);
        
        // Parse and process command
        char cmd[64], args[256];
        if (sscanf(buffer, "%63s %255[^\n]", cmd, args) >= 1) {
            if (strcmp(cmd, "SET_STATE") == 0) {
                uint8_t state;
                if (sscanf(args, "%hhu", &state) == 1) {
                    set_node_state(state);
                    update_led_color();
                    send(client_socket, "OK: State updated\n", 18, 0);
                } else {
                    send(client_socket, "ERROR: Invalid state\n", 22, 0);
                }
            }
            else if (strcmp(cmd, "SET_COLOR") == 0) {
                uint8_t r, g, b;
                if (sscanf(args, "%hhu,%hhu,%hhu", &r, &g, &b) == 3) {
                    set_led_color(r, g, b);
                    send(client_socket, "OK: Color updated\n", 18, 0);
                } else {
                    send(client_socket, "ERROR: Invalid color\n", 21, 0);
                }
            }
            else if (strcmp(cmd, "UPDATE_KERNEL") == 0) {
                float values[KERNEL_SIZE];
                int parsed = sscanf(args, "%f,%f,%f,%f,%f,%f,%f,%f,%f",
                                    &values[0], &values[1], &values[2],
                                    &values[3], &values[4], &values[5],
                                    &values[6], &values[7], &values[8]);
                if (parsed == KERNEL_SIZE && set_kernel_values(values)) {
                    send(client_socket, "OK: Kernel updated\n", 19, 0);
                } else if (load_preset(args)) {
                    send(client_socket, "OK: Preset loaded\n", 18, 0);
                } else {
                    send(client_socket, "ERROR: Invalid kernel\n", 22, 0);
                }
            }
            else if (strcmp(cmd, "UPDATE_ACTIVATION") == 0) {
                activation_rule_t rules[MAX_ACTIVATIONS] = {};
                uint8_t count = 0;
                char *cursor = args;
                while (count < MAX_ACTIVATIONS && cursor && *cursor) {
                    int op = 0;
                    float value = 0.0f;
                    int consumed = 0;
                    if (sscanf(cursor, "%d,%f%n", &op, &value, &consumed) < 2 ||
                        op < 0 || op > 4) {
                        break;
                    }
                    rules[count].op = (uint8_t)op;
                    rules[count].value = value;
                    count++;
                    cursor += consumed;
                    if (*cursor == ',') cursor++;
                }
                if (set_activation_rules(rules, count)) {
                    send(client_socket, "OK: Activation updated\n", 23, 0);
                } else {
                    send(client_socket, "ERROR: Invalid activation\n", 26, 0);
                }
            }
            else if (strcmp(cmd, "PRESET") == 0) {
                if (load_preset(args)) {
                    send(client_socket, "OK: Preset loaded\n", 18, 0);
                } else {
                    send(client_socket, "ERROR: Unknown preset\n", 22, 0);
                }
            }
            else if (strcmp(cmd, "RESET_VALUE") == 0) {
                reset_state_value();
                send(client_socket, "OK: Value reset\n", 16, 0);
            }
            else if (strcmp(cmd, "GET_STATUS") == 0) {
                char response[256];
                snprintf(response, sizeof(response), 
                        "STATUS: state=%d, value=%d, sum=%.2f, temp=%d, neighbors=%d, kernel=%s, activations=%s\n",
                        node_state.state, node_state.value, node_state.activation_sum,
                        node_state.temperature, get_neighbor_count(),
                        get_kernel_function(), get_activation_function());
                send(client_socket, response, strlen(response), 0);
            }
            else if (strcmp(cmd, "PING") == 0) {
                send(client_socket, "PONG\n", 5, 0);
            }
            else {
                char response[96];
                snprintf(response, sizeof(response), "ERROR: Unknown cmd '%s'\n", cmd);
                send(client_socket, response, strlen(response), 0);
            }
        }
    }
    
    close(client_socket);
}

/**
 * Send state update to visualization server via UDP
 * Uses the packed UDP message format (77 bytes) expected by server.js
 */
void send_state_update_to_visualization(void) {
    if (udp_socket < 0) {
        ESP_LOGW(TAG, "UDP socket not initialized");
        return;
    }
    
    // Prepare UDP message in the format expected by visualization server
    prepare_udp_message(&udp_message);
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(VISUALIZATION_PORT);
    inet_pton(AF_INET, VISUALIZATION_IP, &dest_addr.sin_addr.s_addr);
    
    ssize_t bytes_sent = sendto(udp_socket, &udp_message, sizeof(udp_message_t), 0,
                               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (bytes_sent == sizeof(udp_message_t)) {
        ESP_LOGD(TAG, "State update sent to visualization server (%d bytes)", bytes_sent);
    } else {
        ESP_LOGE(TAG, "Failed to send state update: %d (%d bytes sent, expected %d)",
                 errno, bytes_sent, sizeof(udp_message_t));
    }
}

/**
 * Check if we have an IP address (connected to router)
 */
bool has_ip_address(void) {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return false;
    }
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        return ip_info.ip.addr != 0;
    }
    return false;
}

static bool state_publish_relevant_change(const node_state_t *previous, const node_state_t *current) {
    if (previous == NULL || current == NULL) {
        return true;
    }

    return previous->state != current->state ||
           previous->value != current->value ||
           previous->temperature != current->temperature ||
           previous->mmwave_presence != current->mmwave_presence ||
           previous->mmwave_distance != current->mmwave_distance ||
           previous->kernel_sequence != current->kernel_sequence ||
           previous->value_sequence != current->value_sequence ||
           previous->activation_sequence != current->activation_sequence;
}

static void init_nvs_storage(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs recovery (%s), erasing NVS partition", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static bool mac_is_zero(const uint8_t *mac) {
    static const uint8_t zero_mac[6] = {0};
    return mac == NULL || memcmp(mac, zero_mac, sizeof(zero_mac)) == 0;
}

static void check_mesh_health(uint32_t now) {
    if (now - last_mesh_health_check_ms < MESH_HEALTH_CHECK_INTERVAL_MS) {
        return;
    }
    last_mesh_health_check_ms = now;

    if (!mesh_initialized) {
        ESP_LOGW(TAG, "Mesh is not initialized; retrying mesh initialization");
        init_mesh();
        return;
    }

    int layer = esp_mesh_get_layer();
    if (layer > 0) {
        last_mesh_attached_ms = now;
        return;
    }

    if (last_mesh_attached_ms == 0) {
        last_mesh_attached_ms = now;
        return;
    }

    if (now - last_mesh_attached_ms >= MESH_RECONNECT_RESTART_MS) {
        ESP_LOGW(TAG, "Mesh has been detached for %lu ms; restarting mesh stack",
                 (unsigned long)(now - last_mesh_attached_ms));
        has_current_root_addr = false;
        mesh_initialized = false;
        esp_err_t err = esp_mesh_stop();
        if (err != ESP_OK && err != ESP_ERR_MESH_NOT_START) {
            ESP_LOGW(TAG, "Failed to stop mesh before restart: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        init_mesh();
        last_mesh_attached_ms = now;
    }
}

/**
 * Application main entry point for ESP-IDF
 */
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32 Mesh Node - Starting up");
    ESP_LOGI(TAG, "Platform: %s", PLATFORM_NAME);
    ESP_LOGI(TAG, "WiFi Channel: %d", WIFI_CHANNEL);
    
    // Initialize NVS
    init_nvs_storage();
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Initialize event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create WiFi mesh netif
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&mesh_netif_sta, NULL));
    
    // Initialize WiFi
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    
    // Configure WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    if (WIFI_CHANNEL > 0) {
        ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_initialized = true;
    
    // Get MAC address
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, node_mac));
    if (mac_is_zero(node_mac)) {
        ESP_LOGE(TAG, "WiFi returned a zero MAC; erasing NVS and restarting");
        ESP_ERROR_CHECK(nvs_flash_erase());
        esp_restart();
    }
    ESP_LOGI(TAG, "MAC Address: " MACSTR, MAC2STR(node_mac));
    
    // Initialize platform
    platform_init();
    
    // Initialize state manager
    init_state();
    
    // Initialize neighbor list
    init_neighbor_list();
    
    // Initialize mesh
    init_mesh();
    
    // Initialize UDP socket
    init_udp();
    
    ESP_LOGI(TAG, "Initialization complete. Mesh is running.");
    
    // Main loop
    uint32_t last_state_update = 0;
    uint32_t last_mesh_state_publish = 0;
#if ENABLE_WIFI_NEIGHBOR_SCAN
    uint32_t last_neighbor_scan = 0;
#endif
    uint32_t last_visualization_update = 0;
    node_state_t last_published_state = {};
    bool has_published_state = false;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        
        uint32_t now = esp_timer_get_time() / 1000; // ms

        check_mesh_health(now);
        
        // Update state (read sensors, etc.)
        if (now - last_state_update >= STATE_UPDATE_INTERVAL) {
            last_state_update = now;
            update_state();
            update_led_color();

            bool state_changed = !has_published_state ||
                                 state_publish_relevant_change(&last_published_state, &node_state);
            bool periodic_due = now - last_mesh_state_publish >= MQTT_UPDATE_INTERVAL_MS;
            if (state_changed || periodic_due) {
                last_mesh_state_publish = now;
                last_published_state = node_state;
                has_published_state = true;
                send_state_to_neighbors();
#if ENABLE_MQTT_VISUALIZATION
                if (esp_mesh_is_root() && is_mqtt_connected()) {
                    mqtt_publish_topology_and_state();
                }
#endif
            }
        }
        
        // Trigger neighbor discovery
#if ENABLE_WIFI_NEIGHBOR_SCAN
        if (mesh_initialized && esp_mesh_get_layer() > 0 && now - last_neighbor_scan >= BEACON_SCAN_INTERVAL) {
            last_neighbor_scan = now;
            trigger_neighbor_discovery();
            cleanup_inactive_neighbors();
        }
#endif
        
        // Send updates to visualization server
        bool can_send_to_viz = has_ip_address() || esp_mesh_is_root();
        if (can_send_to_viz && now - last_visualization_update >= 1000) {
            last_visualization_update = now;
            send_state_update_to_visualization();
        }
        
        // Handle TCP clients
        if (tcp_server_socket >= 0) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(tcp_server_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket >= 0) {
                handle_tcp_client(client_socket);
            }
        }
        
        // Log mesh status periodically
        if (now % 5000 == 0) {
            int layer = esp_mesh_get_layer();
            int rtable_size = esp_mesh_get_routing_table_size();
            ESP_LOGI(TAG, "Layer: %d, Routing table: %d, Neighbors: %d", 
                     layer, rtable_size, get_neighbor_count());
        }
    }
}
