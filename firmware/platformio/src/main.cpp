/**
 * Main Entry Point for ESP32 Mesh Node
 * Adaptive mesh topology with UDP unicast and dynamic TX power
 * PlatformIO compatible for VS Code
 * Supports: ESP32-C3 (2.4 GHz), ESP32-C5 (5 GHz), Generic ESP32
 */

#include "mesh_node.h"
#include "beacon_monitor.h"
#include "state_manager.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "MAIN";

// ============================================================================
// UDP Receive Task
// ============================================================================

/**
 * Task to receive UDP messages
 */
void udp_receive_task(void *pvParameters) {
    ESP_LOGI(TAG, "UDP receive task started");
    
    while (1) {
        if (udp_socket < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        uint8_t buffer[UDP_MTU];
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        
        int len = recvfrom(udp_socket, buffer, sizeof(buffer), 0,
                          (struct sockaddr *)&from, &from_len);
        
        if (len > 0) {
            // Process received message
            if (len >= sizeof(mesh_message_t)) {
                mesh_message_t *msg = (mesh_message_t *)buffer;
                
                // Verify checksum
                if (verify_checksum(msg)) {
                    // Check if this is from a neighbor
                    bool is_neighbor = false;
                    for (int i = 0; i < MAX_NEIGHBORS; i++) {
                        if (neighbor_list[i].active && 
                            memcmp(neighbor_list[i].mac, msg->mac, 6) == 0) {
                            is_neighbor = true;
                            break;
                        }
                    }
                    
                    if (!is_neighbor) {
                        // Forward non-neighbor messages to visualization
                        forward_to_visualization(msg);
                    }
                    
                    if (DEBUG_UDP) {
                        ESP_LOGD(TAG, "Received UDP from: ");
                        print_mac(msg->mac);
                        ESP_LOGD(TAG, " Type: %d, State: %d", msg->msg_type, msg->state.state);
                    }
                } else {
                    ESP_LOGW(TAG, "Invalid checksum from: %d.%d.%d.%d:%d",
                             from.sin_addr.s_addr & 0xFF, (from.sin_addr.s_addr >> 8) & 0xFF,
                             (from.sin_addr.s_addr >> 16) & 0xFF, (from.sin_addr.s_addr >> 24) & 0xFF,
                             ntohs(from.sin_port));
                }
            }
        } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGE(TAG, "UDP receive error: %d", errno);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================================
// Main Application
// ============================================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32 Mesh Node - Adaptive Topology");
    ESP_LOGI(TAG, "  Platform: %s", PLATFORM_NAME);
    ESP_LOGI(TAG, "  WiFi Channel: %d", WIFI_CHANNEL);
    ESP_LOGI(TAG, "  Supports 2.4GHz: %s", SUPPORTS_24GHZ ? "Yes" : "No");
    ESP_LOGI(TAG, "  Supports 5GHz: %s", SUPPORTS_5GHZ ? "Yes" : "No");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize platform-specific hardware
    platform_init();
    
    // Initialize mesh node
    init_mesh();
    
    // Set node to active state
    set_node_state(NODE_STATE_ACTIVE);
    
    // Create UDP receive task
    xTaskCreate(udp_receive_task, "udp_receive", 4096, NULL, 5, NULL);
    
    // Main loop (mostly idle)
    while (1) {
        // Check mesh status periodically
        if (mesh_initialized) {
            mesh_status_t status;
            if (esp_mesh_get_status(&status) == ESP_OK) {
                if (status.mesh_state == MESH_STATE_ROOT) {
                    // Root node behavior
                } else if (status.mesh_state == MESH_STATE_IDLE) {
                    ESP_LOGW(TAG, "Mesh is idle, attempting to reconnect...");
                    esp_mesh_start();
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
