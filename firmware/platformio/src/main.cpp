/**
 * Main Entry Point for ESP32 Mesh Node
 * ESP-IDF Framework with ESP-WiFi-Mesh
 * Seeed Studio XIAO ESP32-C3
 */

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#include "config.h"

static const char *TAG = "MAIN";

// Global MAC address
uint8_t node_mac[6] = {0};

/**
 * IP event handler
 */
void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));
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
            break;
        }
        case MESH_EVENT_PARENT_DISCONNECTED: {
            mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d", disconnected->reason);
            break;
        }
        case MESH_EVENT_LAYER_CHANGE: {
            mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
            ESP_LOGI(TAG, "<MESH_EVENT_LAYER_CHANGE>new_layer:%d", layer_change->new_layer);
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
 * Application main entry point for ESP-IDF
 */
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32 Mesh Node - Starting up");
    ESP_LOGI(TAG, "Platform: %s", PLATFORM_NAME);
    ESP_LOGI(TAG, "WiFi Channel: %d", WIFI_CHANNEL);
    
    // Initialize NVS (required for WiFi)
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Initialize event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create WiFi mesh netif
    esp_netif_t *netif_sta = NULL;
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));
    
    // Initialize WiFi
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    
    // Configure WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Get MAC address
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, node_mac));
    ESP_LOGI(TAG, "MAC Address: " MACSTR, MAC2STR(node_mac));
    
    // Initialize mesh
    ESP_ERROR_CHECK(esp_mesh_init());
    
    // Set mesh topology and parameters
    ESP_ERROR_CHECK(esp_mesh_set_topology(MESH_TOPO_TREE));
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(MESH_MAX_HOPS));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(MESH_VOTE_PERCENT));
    ESP_ERROR_CHECK(esp_mesh_disable_ps());
    
    // Set mesh configuration
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    cfg.channel = WIFI_CHANNEL;
    memcpy((uint8_t *) &cfg.router.ssid, MESH_ROUTER_SSID, strlen(MESH_ROUTER_SSID));
    memcpy((uint8_t *) &cfg.router.password, MESH_ROUTER_PASS, strlen(MESH_ROUTER_PASS));
    memcpy((uint8_t *) &cfg.mesh_id, (uint8_t*)"glow_mesh", 10);
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    
    // Start mesh
    ESP_ERROR_CHECK(esp_mesh_start());
    
    ESP_LOGI(TAG, "Initialization complete. Mesh is running.");
    
    // Main loop - monitor mesh status
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (esp_mesh_is_root()) {
            ESP_LOGI(TAG, "Node is ROOT");
        }
        
        int layer = esp_mesh_get_layer();
        int rtable_size = esp_mesh_get_routing_table_size();
        ESP_LOGD(TAG, "Layer: %d, Routing table size: %d", layer, rtable_size);
    }
}
