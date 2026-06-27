/**
 * ESP32 Arduino Compatibility Layer
 * Minimal compatibility for ESP-IDF code to work with Arduino framework
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// Include standard C headers
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// ESP-IDF Logging Emulation
// ============================================================================

#define ESP_LOGE(tag, format, ...) Serial.printf("[E][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) Serial.printf("[W][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) Serial.printf("[I][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) if (DEBUG_MESH || DEBUG_BEACON || DEBUG_UDP || DEBUG_NEIGHBORS || DEBUG_TX_POWER) { Serial.printf("[D][%s] " format "\n", tag, ##__VA_ARGS__); }
#define ESP_LOGV(tag, format, ...) Serial.printf("[V][%s] " format "\n", tag, ##__VA_ARGS__)

// ============================================================================
// ESP-IDF Error Codes
// ============================================================================

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

const char* esp_err_to_name(esp_err_t err) {
    return err == ESP_OK ? "ESP_OK" : "ERROR";
}

#define ESP_ERROR_CHECK(x) do { \
    esp_err_t err_rc_ = (x); \
    if (err_rc_ != ESP_OK) { \
        ESP_LOGE("ESP_ERROR_CHECK", "Error: %s at %s:%d", esp_err_to_name(err_rc_), __FILE__, __LINE__); \
    } \
} while(0)

// ============================================================================
// ESP-IDF Mesh Types and API Emulation
// ============================================================================

typedef enum {
    MESH_STATE_IDLE = 0,
    MESH_STATE_ROOT,
    MESH_STATE_NONROOT,
    MESH_STATE_MAX
} mesh_state_t;

typedef struct {
    mesh_state_t mesh_state;
    uint8_t mac[6];
    uint8_t parent_mac[6];
    uint8_t layer;
    uint8_t child_num;
    uint8_t max_layer;
    uint8_t max_child_num;
} mesh_status_t;

typedef enum {
    MESH_EVENT_ROOT_GOT_IP = 0,
    MESH_EVENT_ROOT_LOST_IP,
    MESH_EVENT_ROOT_CONNECTED,
    MESH_EVENT_ROOT_DISCONNECTED,
    MESH_EVENT_NO_PARENT,
    MESH_EVENT_PARENT_CONNECTED,
    MESH_EVENT_PARENT_DISCONNECTED,
    MESH_EVENT_LAYER_CHANGE,
    MESH_EVENT_CHILD_CONNECTED,
    MESH_EVENT_CHILD_DISCONNECTED,
    MESH_EVENT_MAX
} mesh_event_id_t;

typedef struct {
    mesh_event_id_t id;
    void* data;
} mesh_event_t;

typedef esp_err_t (*mesh_event_cb_t)(mesh_event_t* event);

typedef struct {
    uint8_t channel;
    struct {
        uint8_t ssid[32];
        uint8_t password[64];
        uint8_t bssid[6];
    } router;
    uint8_t mesh_id[32];
    struct {
        uint8_t max_connection;
        uint8_t nonmesh_ap_table_size;
        bool allow_router_switch;
        bool allow_channel_switch;
        uint8_t max_hop;
        uint8_t vote_percentage;
        uint8_t backoff_exponent;
        uint8_t max_retries;
        uint16_t beacon_interval;
        uint8_t listen_interval;
        uint32_t nonmesh_ap_probe_interval;
        uint32_t mesh_ap_idle_interval;
        uint32_t mesh_ap_max_age;
        uint32_t mesh_ap_fail_interval;
        uint32_t mesh_ap_select_interval;
        uint32_t mesh_ap_select_random_interval;
        uint32_t mesh_ap_switch_interval;
        uint32_t mesh_ap_switch_random_interval;
        uint16_t mesh_ap_beacon_interval;
        uint8_t mesh_ap_listen_interval;
        uint32_t mesh_ap_scan_interval;
    } mesh_ap;
} mesh_cfg_t;

// Mesh function stubs
static bool mesh_initialized_arduino = false;

esp_err_t esp_mesh_set_config(mesh_cfg_t* config) {
    return ESP_OK;
}

esp_err_t esp_mesh_start(void) {
    mesh_initialized_arduino = true;
    return ESP_OK;
}

esp_err_t esp_mesh_stop(void) {
    mesh_initialized_arduino = false;
    return ESP_OK;
}

esp_err_t esp_mesh_get_status(mesh_status_t* status) {
    if (!status) return ESP_FAIL;
    
    if (mesh_initialized_arduino) {
        status->mesh_state = MESH_STATE_NONROOT;
        uint8_t mac[6];
        WiFi.macAddress(mac);
        for (int i = 0; i < 6; i++) {
            status->mac[i] = mac[i];
        }
        status->layer = 1;
        status->child_num = 0;
        status->max_layer = 10;
        status->max_child_num = 6;
    } else {
        status->mesh_state = MESH_STATE_IDLE;
    }
    return ESP_OK;
}

esp_err_t esp_mesh_register_tx_cb(mesh_event_cb_t cb) {
    return ESP_OK;
}

esp_err_t esp_mesh_register_rx_cb(mesh_event_cb_t cb) {
    return ESP_OK;
}

// ============================================================================
// ESP-IDF Network Interface Emulation
// ============================================================================

esp_err_t esp_netif_init(void) {
    return ESP_OK;
}

esp_err_t esp_netif_create_default_wifi_mesh_netifs(int count) {
    return ESP_OK;
}

// ============================================================================
// ESP-IDF Event Loop Emulation
// ============================================================================


typedef struct {
    int event_base;
    int event_id;
    void* event_data;
} esp_event_t;

typedef void (*esp_event_handler_func_t)(void* arg, int event_base, int event_id, void* event_data);

esp_err_t esp_event_loop_create_default(void) {
    return ESP_OK;
}

esp_err_t esp_event_handler_instance_register(int event_base, int event_id, esp_event_handler_func_t handler, void* arg, esp_event_handler_t* instance) {
    return ESP_OK;
}

// ============================================================================
// ESP-IDF Timer Emulation
// ============================================================================

#include <esp_timer.h>

typedef void (*esp_timer_cb_t)(void* arg);

esp_err_t esp_timer_create(const void* create_args, esp_timer_handle_t* out_handle) {
    *out_handle = NULL;
    return ESP_OK;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t handle, uint64_t timeout_us) {
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t handle, uint64_t period_us) {
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t handle) {
    return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t handle) {
    return ESP_OK;
}

// ============================================================================
// ESP-IDF ADC Emulation
// ============================================================================

typedef void* adc_oneshot_unit_handle_t;

typedef struct {
    int channel;
    int atten;
    int bitwidth;
} adc_oneshot_chan_cfg_t;

esp_err_t adc_oneshot_new_unit(void* init_config, adc_oneshot_unit_handle_t* out_handle) {
    *out_handle = (adc_oneshot_unit_handle_t)1;
    return ESP_OK;
}

esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle, adc_oneshot_chan_cfg_t* chan_cfg) {
    return ESP_OK;
}

esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle, int channel, int* out_value) {
    if (out_value) {
        *out_value = analogRead(channel);
    }
    return ESP_OK;
}

esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t handle) {
    return ESP_OK;
}

// ============================================================================
// ESP-IDF LEDC (PWM) Emulation
// ============================================================================

typedef enum {
    LEDC_LOW_SPEED_MODE = 0,
    LEDC_HIGH_SPEED_MODE
} ledc_mode_t;

typedef enum {
    LEDC_TIMER_0 = 0,
    LEDC_TIMER_1,
    LEDC_TIMER_2,
    LEDC_TIMER_3
} ledc_timer_t;

typedef enum {
    LEDC_CHANNEL_0 = 0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3,
    LEDC_CHANNEL_4,
    LEDC_CHANNEL_5,
    LEDC_CHANNEL_6,
    LEDC_CHANNEL_7
} ledc_channel_t;

typedef enum {
    LEDC_TIMER_8_BIT = 8,
    LEDC_TIMER_12_BIT = 12
} ledc_timer_bit_t;

typedef enum {
    LEDC_AUTO_CLK = 0
} ledc_clk_cfg_t;

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_bit_t duty_resolution;
    ledc_timer_t timer_num;
    uint32_t freq_hz;
    ledc_clk_cfg_t clk_cfg;
} ledc_timer_config_t;

typedef enum {
    LEDC_INTR_DISABLE = 0
} ledc_intr_type_t;

typedef struct {
    int gpio_num;
    ledc_mode_t speed_mode;
    ledc_channel_t channel;
    ledc_intr_type_t intr_type;
    ledc_timer_t timer_sel;
    uint32_t duty;
    int hpoint;
} ledc_channel_config_t;

esp_err_t ledc_timer_config(ledc_timer_config_t* timer) {
    return ESP_OK;
}

esp_err_t ledc_channel_config(ledc_channel_config_t* channel) {
    pinMode(channel->gpio_num, OUTPUT);
    return ESP_OK;
}

esp_err_t ledc_set_duty(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty, uint32_t hpoint) {
    return ESP_OK;
}

esp_err_t ledc_update_duty(ledc_mode_t speed_mode, ledc_channel_t channel) {
    return ESP_OK;
}

// ============================================================================
// FreeRTOS Compatibility
// ============================================================================

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define pdMS_TO_TICKS(ms) ((ms) / portTICK_PERIOD_MS)
#define pdTICKS_TO_MS(ticks) ((ticks) * portTICK_PERIOD_MS)
