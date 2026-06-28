/**
 * ESP32 Arduino Compatibility Layer
 * Provides compatibility macros for ESP-IDF
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

// ESP-IDF includes
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mesh.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"

// Logging macros
#define ESP_LOGE(tag, format, ...) esp_log_write(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) esp_log_write(ESP_LOG_WARN, tag, format, ##____VA_ARGS__)
#define ESP_LOGI(tag, format, ...) esp_log_write(ESP_LOG_INFO, tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...)
#define ESP_LOGV(tag, format, ...)

// Error codes
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

// ESP_ERROR_CHECK macro
#define ESP_ERROR_CHECK(x) do { \
    esp_err_t err_rc_ = (x); \
    if (err_rc_ != ESP_OK) { \
        ESP_LOGE("ERR", "Error: %d at %s:%d", err_rc_, __FILE__, __LINE__); \
    } \
} while(0)

// millis() compatibility
#define millis() (esp_timer_get_time() / 1000)

// delay() compatibility
#define delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))

// IPAddress compatibility for ESP-IDF
struct IPAddress {
    uint8_t bytes[4];
    bool fromString(const char* str) {
        return sscanf(str, "%hhu.%hhu.%hhu.%hhu", &bytes[0], &bytes[1], &bytes[2], &bytes[3]) == 4;
    }
    operator uint32_t() const {
        return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    }
    uint8_t operator[](int index) const { return bytes[index]; }
    uint8_t& operator[](int index) { return bytes[index]; }
};

// FreeRTOS compatibility
#define pdMS_TO_TICKS(ms) ((ms) / portTICK_PERIOD_MS)
