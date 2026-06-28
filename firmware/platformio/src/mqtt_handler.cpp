/**
 * MQTT Handler for Mesh Visualization
 * Publishes mesh topology and state to MQTT broker for visualization app
 */

#include "mqtt_handler.h"
#include "config.h"
#include "mesh_node.h"
#include "state_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_mesh.h"
#include "mqtt_client.h"
#include "cJSON.h"

static const char *TAG = "MQTT_HANDLER";

// MQTT Client Handle
esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

/**
 * MQTT Event Handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_connected = true;
            // Publish topology immediately on connection
            mqtt_publish_topology_and_state();
            // Subscribe to any necessary topics here
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_connected = false;
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            mqtt_connected = false;
            break;
            
        default:
            ESP_LOGI(TAG, "Other MQTT event: %d", event->event_id);
            break;
    }
}

/**
 * Initialize MQTT Client
 */
void init_mqtt(void) {
    if (mqtt_client != NULL) {
        ESP_LOGI(TAG, "MQTT already initialized");
        return;
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {0};
    mqtt_cfg.broker.address.uri = MQTT_BROKER_IP;
    mqtt_cfg.broker.address.port = MQTT_BROKER_PORT;
    mqtt_cfg.credentials.client_id = MQTT_CLIENT_ID;
    mqtt_cfg.task.priority = 5;
    mqtt_cfg.task.stack_size = 6144;
    mqtt_cfg.buffer.size = 1024 * 2;
    mqtt_cfg.network.timeout_ms = 10000;
    mqtt_cfg.network.reconnect_timeout_ms = 10000;
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }
    
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "MQTT client initialized and started");
}

/**
 * Check if MQTT is connected
 */
bool is_mqtt_connected(void) {
    return mqtt_connected;
}

/**
 * Disconnect MQTT Client
 */
void mqtt_disconnect(void) {
    if (mqtt_client != NULL) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        mqtt_connected = false;
        ESP_LOGI(TAG, "MQTT client disconnected and destroyed");
    }
}

/**
 * Publish mesh state as JSON to MQTT
 */
void mqtt_publish_state(void) {
    if (!mqtt_connected || mqtt_client == NULL) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish state");
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return;
    }
    
    // Add node MAC address
    char node_mac_str[18];
    extern uint8_t node_mac[6];
    snprintf(node_mac_str, sizeof(node_mac_str), MACSTR, MAC2STR(node_mac));
    cJSON_AddStringToObject(root, "mac", node_mac_str);
    
    // Add node state
    cJSON_AddNumberToObject(root, "state", node_state.state);
    cJSON_AddNumberToObject(root, "temperature", node_state.temperature);
    cJSON_AddNumberToObject(root, "mmwave_presence", node_state.mmwave_presence);
    cJSON_AddNumberToObject(root, "mmwave_distance", node_state.mmwave_distance);
    cJSON_AddNumberToObject(root, "timestamp", node_state.timestamp);
    
    // Add LED color
    cJSON *color = cJSON_CreateArray();
    cJSON_AddItemToArray(color, cJSON_CreateNumber(node_state.color[0]));
    cJSON_AddItemToArray(color, cJSON_CreateNumber(node_state.color[1]));
    cJSON_AddItemToArray(color, cJSON_CreateNumber(node_state.color[2]));
    cJSON_AddItemToObject(root, "color", color);
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str != NULL) {
        int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_STATE_TOPIC, json_str, 0, 0, 0);
        ESP_LOGD(TAG, "Published state to %s [msg_id=%d]: %s", MQTT_STATE_TOPIC, msg_id, json_str);
        free(json_str);
    }
    
    cJSON_Delete(root);
}

/**
 * Publish topology and state to MQTT
 * This is called from external code (like WiFi scan callback)
 */
void mqtt_publish_topology_and_state(void) {
    if (is_mqtt_connected()) {
        mqtt_publish_topology();
        mqtt_publish_state();
    }
}

/**
 * Publish mesh topology (node + neighbors) as JSON to MQTT
 */
void mqtt_publish_topology(void) {
    if (!mqtt_connected || mqtt_client == NULL) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish topology");
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return;
    }
    
    // Add node information
    char node_mac_str[18];
    extern uint8_t node_mac[6];
    snprintf(node_mac_str, sizeof(node_mac_str), MACSTR, MAC2STR(node_mac));
    cJSON_AddStringToObject(root, "node_mac", node_mac_str);
    cJSON_AddNumberToObject(root, "node_state", node_state.state);
    
    // Get mesh layer and routing table size
    int layer = 0;
    int rtable_size = 0;
    // These are ESP-WiFi-Mesh specific functions
    if (mesh_initialized) {
        layer = esp_mesh_get_layer();
        rtable_size = esp_mesh_get_routing_table_size();
    }
    cJSON_AddNumberToObject(root, "layer", layer);
    cJSON_AddNumberToObject(root, "routing_table_size", rtable_size);
    
    // Add neighbors array
    cJSON *neighbors = cJSON_CreateArray();
    if (neighbors != NULL) {
        for (int i = 0; i < MAX_NEIGHBORS; i++) {
            if (neighbor_list[i].active) {
                cJSON *neighbor = cJSON_CreateObject();
                if (neighbor != NULL) {
                    char mac_str[18];
                    snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(neighbor_list[i].mac));
                    cJSON_AddStringToObject(neighbor, "mac", mac_str);
                    cJSON_AddNumberToObject(neighbor, "rssi", neighbor_list[i].rssi);
                    cJSON_AddNumberToObject(neighbor, "last_seen", neighbor_list[i].last_seen);
                    cJSON_AddItemToArray(neighbors, neighbor);
                }
            }
        }
        cJSON_AddItemToObject(root, "neighbors", neighbors);
    }
    
    // Add neighbor count
    cJSON_AddNumberToObject(root, "neighbor_count", get_neighbor_count());
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str != NULL) {
        int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPOLOGY_TOPIC, json_str, 0, 0, 0);
        ESP_LOGD(TAG, "Published topology to %s [msg_id=%d]: %s", MQTT_TOPOLOGY_TOPIC, msg_id, json_str);
        free(json_str);
    }
    
    cJSON_Delete(root);
}
