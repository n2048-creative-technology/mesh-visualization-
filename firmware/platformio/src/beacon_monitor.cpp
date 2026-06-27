/**
 * Beacon Monitor Implementation
 * Handles promiscuous mode scanning and RSSI tracking for neighbor discovery
 * PlatformIO compatible for VS Code
 */

#include "beacon_monitor.h"
#include "mesh_node.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "BEACON_MONITOR";

// Global variables
bool beacon_monitoring_active = false;
uint32_t last_beacon_scan_time = 0;

// ============================================================================
// Initialization Functions
// ============================================================================

/**
 * Initialize beacon monitoring
 */
void init_beacon_monitoring(void) {
    ESP_LOGI(TAG, "Initializing beacon monitoring");
    
    // Start promiscuous mode
    start_promiscuous_mode();
    
    beacon_monitoring_active = true;
    last_beacon_scan_time = esp_timer_get_time() / 1000; // Convert to ms
    
    ESP_LOGI(TAG, "Beacon monitoring initialized");
}

/**
 * Start promiscuous mode for beacon frame capture
 */
void start_promiscuous_mode(void) {
    ESP_LOGI(TAG, "Starting promiscuous mode");
    
    // Set promiscuous mode callback
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_filter(&wifi_promiscuous_callback);
    
    // Enable promiscuous mode
    esp_err_t err = esp_wifi_set_promiscuous(true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable promiscuous mode: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "Promiscuous mode enabled");
}

/**
 * Stop promiscuous mode
 */
void stop_promiscuous_mode(void) {
    ESP_LOGI(TAG, "Stopping promiscuous mode");
    
    esp_err_t err = esp_wifi_set_promiscuous(false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable promiscuous mode: %s", esp_err_to_name(err));
        return;
    }
    
    beacon_monitoring_active = false;
    ESP_LOGI(TAG, "Promiscuous mode disabled");
}

// ============================================================================
// Beacon Processing Functions
// ============================================================================

/**
 * Process a received beacon frame
 */
void process_beacon_frame(uint8_t *frame, uint16_t len, int8_t rssi) {
    beacon_info_t beacon;
    
    // Extract beacon information
    if (!extract_beacon_info(frame, len, &beacon)) {
        return; // Not a valid beacon frame
    }
    
    // Check if this is a mesh node beacon
    if (!is_mesh_beacon(frame, len)) {
        return;
    }
    
    // Ignore our own beacons
    if (memcmp(beacon.mac, node_mac, 6) == 0) {
        return;
    }
    
    // Update neighbor list with this beacon
    update_neighbor_list(beacon.mac, beacon.rssi);
    
    if (DEBUG_BEACON) {
        ESP_LOGD(TAG, "Processed beacon from: ");
        print_mac(beacon.mac);
        ESP_LOGD(TAG, " RSSI: %d dBm", beacon.rssi);
    }
}

/**
 * Extract beacon frame information
 */
bool extract_beacon_info(uint8_t *frame, uint16_t len, beacon_info_t *beacon) {
    if (len < 40) { // Minimum beacon frame size
        return false;
    }
    
    // Frame control (2 bytes) + Duration (2 bytes) + Address fields (24 bytes)
    uint8_t *frame_body = frame + 24;
    uint16_t frame_len = len - 24;
    
    // Beacon frame type check (0x80 = Beacon)
    if ((frame[0] & 0x0F) != 0x08) {
        return false;
    }
    
    // Extract BSSID (AP MAC) - Address 3 in beacon frame
    memcpy(beacon->mac, frame + 16, 6);
    
    // Extract RSSI (passed as parameter)
    beacon->rssi = rssi;
    
    // Extract channel from beacon frame
    // Channel is in the DS Parameter Set (usually at offset 36 from frame start)
    if (frame_len > 12) {
        uint8_t *tag = frame_body + 12; // Skip timestamp, beacon interval, capability
        uint16_t tag_len = frame_len - 12;
        
        while (tag_len > 2) {
            uint8_t tag_num = tag[0];
            uint8_t tag_length = tag[1];
            
            if (tag_num == 3) { // DS Parameter Set
                beacon->channel = tag[2];
                break;
            }
            
            tag += tag_length + 2;
            tag_len -= tag_length + 2;
        }
    }
    
    // Extract SSID
    beacon->ssid_len = 0;
    memset(beacon->ssid, 0, 33);
    
    uint8_t *tag = frame_body + 12; // Skip fixed fields
    uint16_t remaining = frame_len - 12;
    
    while (remaining > 2) {
        uint8_t tag_num = tag[0];
        uint8_t tag_len = tag[1];
        
        if (tag_num == 0) { // SSID tag
            if (tag_len > 0 && tag_len <= 32) {
                memcpy(beacon->ssid, tag + 2, tag_len);
                beacon->ssid[tag_len] = '\0';
                beacon->ssid_len = tag_len;
            }
            break;
        }
        
        tag += tag_len + 2;
        remaining -= tag_len + 2;
    }
    
    return true;
}

// ============================================================================
// Callback Functions
// ============================================================================

/**
 * WiFi promiscuous mode callback
 * Called for every received frame
 */
void wifi_promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    
    // Only process management frames (beacons)
    if (type != WIFI_PKT_MGMT || pkt->payload[0] != 0x80) {
        return;
    }
    
    // Process beacon frame
    int8_t rssi = pkt->rx_ctrl.rssi;
    process_beacon_frame(pkt->payload, pkt->rx_ctrl.sig_len, rssi);
    
    // Update last scan time
    last_beacon_scan_time = esp_timer_get_time() / 1000;
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Check if frame is from a mesh node
 */
bool is_mesh_beacon(uint8_t *frame, uint16_t len) {
    // For now, accept all beacons
    // In production, you might check for a specific SSID or vendor OUI
    return true;
}

/**
 * Validate MAC address
 */
bool is_valid_mac(uint8_t *mac) {
    if (mac == NULL) return false;
    
    // Check for broadcast/multicast addresses
    if (mac[0] & 0x01) return false; // Multicast
    if (memcmp(mac, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0) return false; // Broadcast
    if (memcmp(mac, "\x00\x00\x00\x00\x00\x00", 6) == 0) return false; // Null
    
    return true;
}

/**
 * Print beacon information for debugging
 */
void print_beacon_info(beacon_info_t *beacon) {
    ESP_LOGD(TAG, "Beacon: MAC=");
    print_mac(beacon->mac);
    ESP_LOGD(TAG, ", RSSI=%d, Channel=%d, SSID=%s", 
             beacon->rssi, beacon->channel, beacon->ssid);
}
