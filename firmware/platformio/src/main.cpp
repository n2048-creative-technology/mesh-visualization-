/**
 * Main Entry Point for ESP32 Mesh Node
 * Simplified version for Arduino framework
 */

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    
    Serial.println("ESP32 Mesh Node - Starting up...");
    Serial.printf("Platform: %s\n", PLATFORM_NAME);
    Serial.printf("WiFi Channel: %d\n", WIFI_CHANNEL);
    Serial.printf("Supports 2.4GHz: %s\n", SUPPORTS_24GHZ ? "Yes" : "No");
    Serial.printf("Supports 5GHz: %s\n", SUPPORTS_5GHZ ? "Yes" : "No");
    
    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    
    // Print MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    Serial.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Connect to WiFi
    WiFi.begin(MESH_ROUTER_SSID, MESH_ROUTER_PASS);
    
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nConnected to WiFi");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    
    // Test node state
    node_state_t test_state;
    test_state.state = NODE_STATE_ACTIVE;
    test_state.color[0] = 0;
    test_state.color[1] = 255;
    test_state.color[2] = 0;
    test_state.temperature = 250; // 25.0 * 10
    test_state.mmwave_presence = 0;
    test_state.mmwave_distance = 0;
    test_state.timestamp = 0;
    
    Serial.printf("Node state: %d\n", test_state.state);
    Serial.printf("Temperature: %d\n", test_state.temperature);
}

void loop() {
    delay(1000);
    Serial.println("Main loop running");
}
