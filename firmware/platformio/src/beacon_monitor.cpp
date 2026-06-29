#include "beacon_monitor.h"
#include "config.h"
#include "mesh_node.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"

static const char *TAG = "BEACON_MONITOR";

bool beacon_monitoring_active = false;
uint32_t last_beacon_scan_time = 0;

static bool promiscuous_mode_active = false;
static const uint8_t mesh_id_pattern[6] = {0x47, 0x4c, 0x4f, 0x57, 0x20, 0x01};

static bool mac_has_espressif_oui(const uint8_t *mac) {
    if (mac == NULL) return false;

    static const uint8_t oui_list[][3] = {
        {0x24, 0x6f, 0x28},
        {0x30, 0xae, 0xa4},
        {0x7c, 0xdf, 0xa1},
        {0x84, 0xf3, 0xeb},
        {0xac, 0x27, 0x6e},
        {0xc8, 0xf0, 0x9e},
        {0xcc, 0xdb, 0xa7},
        {0xdc, 0x54, 0x75},
        {0xe8, 0xf6, 0x0a},
        {0xec, 0x94, 0xcb}
    };

    for (size_t i = 0; i < sizeof(oui_list) / sizeof(oui_list[0]); i++) {
        if (memcmp(mac, oui_list[i], 3) == 0) {
            return true;
        }
    }

    return false;
}

static bool ssid_matches_router(const beacon_info_t *beacon) {
    if (beacon == NULL) return false;

    size_t router_len = strlen(MESH_ROUTER_SSID);
    return router_len > 0 &&
           beacon->ssid_len == router_len &&
           memcmp(beacon->ssid, MESH_ROUTER_SSID, router_len) == 0;
}

static bool frame_contains_mesh_id(uint8_t *frame, uint16_t len) {
    if (frame == NULL || len < sizeof(mesh_id_pattern)) return false;

    for (uint16_t i = 0; i <= len - sizeof(mesh_id_pattern); i++) {
        if (memcmp(frame + i, mesh_id_pattern, sizeof(mesh_id_pattern)) == 0) {
            return true;
        }
    }

    return false;
}

static void normalize_mesh_ap_bssid_to_sta_mac(const uint8_t *bssid, uint8_t *sta_mac) {
    memcpy(sta_mac, bssid, 6);

    for (int i = 5; i >= 0; i--) {
        if (sta_mac[i] > 0) {
            sta_mac[i]--;
            break;
        }
        sta_mac[i] = 0xff;
    }
}

/**
 * Initialize beacon monitoring
 */
void init_beacon_monitoring(void) {
    beacon_monitoring_active = true;
    last_beacon_scan_time = 0;
    ESP_LOGI(TAG, "Passive beacon neighbor discovery initialized");
}

/**
 * Start beacon scanning
 */
void start_beacon_scanning(void) {
    start_promiscuous_mode();
}

/**
 * Start promiscuous mode
 */
void start_promiscuous_mode(void) {
#if ENABLE_BEACON_NEIGHBOR_DISCOVERY
    if (promiscuous_mode_active) {
        return;
    }

    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_err_t err = esp_wifi_set_promiscuous_filter(&filter);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set promiscuous filter: %s", esp_err_to_name(err));
        return;
    }

    err = esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_callback);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set promiscuous callback: %s", esp_err_to_name(err));
        return;
    }

    err = esp_wifi_set_promiscuous(true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start promiscuous mode: %s", esp_err_to_name(err));
        return;
    }

    promiscuous_mode_active = true;
    ESP_LOGI(TAG, "Passive beacon neighbor discovery started");
#endif
}

/**
 * Stop promiscuous mode
 */
void stop_promiscuous_mode(void) {
    if (!promiscuous_mode_active) {
        return;
    }

    esp_err_t err = esp_wifi_set_promiscuous(false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop promiscuous mode: %s", esp_err_to_name(err));
        return;
    }

    promiscuous_mode_active = false;
}

/**
 * Process beacon frame
 */
void process_beacon_frame(uint8_t *frame, uint16_t len, int8_t rssi) {
    beacon_info_t beacon = {};
    if (!extract_beacon_info(frame, len, &beacon)) {
        return;
    }

    if (ssid_matches_router(&beacon)) {
        return;
    }

    if (!is_mesh_beacon(frame, len) && !mac_has_espressif_oui(beacon.mac)) {
        return;
    }

    uint8_t neighbor_mac[6];
    normalize_mesh_ap_bssid_to_sta_mac(beacon.mac, neighbor_mac);
    if (!is_valid_mac(neighbor_mac)) {
        return;
    }

    update_neighbor_list(neighbor_mac, rssi);
    last_beacon_scan_time = esp_timer_get_time() / 1000;
}

/**
 * Process WiFi scan results
 */
void process_scan_results(void) {
}

/**
 * Extract beacon info
 */
bool extract_beacon_info(uint8_t *frame, uint16_t len, beacon_info_t *beacon) {
    if (frame == NULL || beacon == NULL || len < 36) {
        return false;
    }

    uint16_t frame_control = frame[0] | ((uint16_t)frame[1] << 8);
    uint8_t frame_type = (frame_control & 0x000c) >> 2;
    uint8_t frame_subtype = (frame_control & 0x00f0) >> 4;
    if (frame_type != 0 || (frame_subtype != 8 && frame_subtype != 5)) {
        return false;
    }

    memcpy(beacon->mac, frame + 16, 6);
    beacon->ssid_len = 0;
    memset(beacon->ssid, 0, sizeof(beacon->ssid));

    uint16_t offset = 36;
    while (offset + 2 <= len) {
        uint8_t id = frame[offset++];
        uint8_t ie_len = frame[offset++];
        if (offset + ie_len > len) {
            break;
        }

        if (id == 0) {
            beacon->ssid_len = ie_len < 32 ? ie_len : 32;
            memcpy(beacon->ssid, frame + offset, beacon->ssid_len);
            beacon->ssid[beacon->ssid_len] = '\0';
            break;
        }

        offset += ie_len;
    }

    return is_valid_mac(beacon->mac);
}

/**
 * Check if frame is mesh beacon
 */
bool is_mesh_beacon(uint8_t *frame, uint16_t len) {
    return frame_contains_mesh_id(frame, len);
}

/**
 * Validate MAC address
 */
bool is_valid_mac(uint8_t *mac) {
    if (mac == NULL) return false;

    bool all_zero = true;
    bool all_ff = true;
    for (int i = 0; i < 6; i++) {
        all_zero = all_zero && mac[i] == 0x00;
        all_ff = all_ff && mac[i] == 0xff;
    }

    return !all_zero && !all_ff && (mac[0] & 0x01) == 0;
}

/**
 * Print beacon info
 */
void print_beacon_info(beacon_info_t *beacon) {
    if (beacon == NULL) return;
    ESP_LOGI(TAG, "Beacon " MACSTR " RSSI=%d SSID=%.*s",
             MAC2STR(beacon->mac), beacon->rssi,
             beacon->ssid_len, beacon->ssid);
}

/**
 * Update beacon monitoring
 */
void update_beacon_monitoring(void) {
    if (!beacon_monitoring_active) {
        return;
    }

    if (!promiscuous_mode_active) {
        start_promiscuous_mode();
    }
}

/**
 * WiFi promiscuous callback
 */
void wifi_promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (!beacon_monitoring_active || type != WIFI_PKT_MGMT || buf == NULL) {
        return;
    }

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    process_beacon_frame(pkt->payload, pkt->rx_ctrl.sig_len, pkt->rx_ctrl.rssi);
}
