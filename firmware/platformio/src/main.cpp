/**
 * Main Entry Point for ESP32 Mesh Node
 * Simplified version for Arduino framework
 */

#include <Arduino.h>
#include "config.h"
#include "esp32_arduino_compat.h"

static const char *TAG = "MAIN";

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32 Mesh Node - Adaptive Topology");
    ESP_LOGI(TAG, "  Platform: %s", PLATFORM_NAME);
    ESP_LOGI(TAG, "  WiFi Channel: %d", WIFI_CHANNEL);
    ESP_LOGI(TAG, "  Supports 2.4GHz: %s", SUPPORTS_24GHZ ? "Yes" : "No");
    ESP_LOGI(TAG, "  Supports 5GHz: %s", SUPPORTS_5GHZ ? "Yes" : "No");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    
    // Print MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    ESP_LOGI(TAG, "MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Connect to WiFi
    WiFi.begin(MESH_ROUTER_SSID, MESH_ROUTER_PASS);
    
    ESP_LOGI(TAG, "Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    ESP_LOGI(TAG, "Connected to WiFi");
    ESP_LOGI(TAG, "IP Address: %s", WiFi.localIP().toString().c_str());
}

void loop() {
    delay(1000);
    ESP_LOGI(TAG, "Main loop running");
}
