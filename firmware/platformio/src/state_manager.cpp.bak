/**
 * State Manager Implementation
 * Manages node state, sensors, and LED control
 * PlatformIO compatible for VS Code
 */

#include "config.h"  // Include for NODE_STATE_* definitions
#include "state_manager.h"
#include "mesh_node.h"
#include "esp32_arduino_compat.h"

static const char *TAG = "STATE_MANAGER";

// Global variables
uint8_t led_pwm_channels[3] = {0};
bool sensors_initialized = false;

// ============================================================================
// Initialization Functions
// ============================================================================

/**
 * Initialize state manager
 */
void init_state(void) {
    ESP_LOGI(TAG, "Initializing state manager");
    
    // Initialize node state
    memset(&node_state, 0, sizeof(node_state_t));
    node_state.state = NODE_STATE_BOOTING;
    node_state.color[0] = 0;    // Red
    node_state.color[1] = 0;    // Green
    node_state.color[2] = 255;  // Blue
    node_state.temperature = 0;
    node_state.mmwave_presence = 0;
    node_state.mmwave_distance = 0;
    node_state.timestamp = 0;
    
    // Initialize LEDs
    init_leds();
    
    // Initialize sensors
    init_sensors();
    
    // Set initial state
    set_node_state(NODE_STATE_ACTIVE);
    
    sensors_initialized = true;
    ESP_LOGI(TAG, "State manager initialized");
}

/**
 * Initialize LED PWM controllers
 */
void init_leds(void) {
    ESP_LOGI(TAG, "Initializing LEDs");
    
    #if defined(ARDUINO)
    // Arduino framework - use analogWrite for PWM
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);
    
    // Set initial LED state (off)
    set_led_color(0, 0, 0);
    
    ESP_LOGI(TAG, "LEDs initialized (Arduino)");
    
    #else
    // ESP-IDF framework - use LEDC for PWM
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer configuration failed: %s", esp_err_to_name(err));
        return;
    }
    
    // Configure LEDC channels for RGB
    ledc_channel_config_t ledc_channel[3] = {
        {
            .gpio_num = LED_RED_PIN,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0
        },
        {
            .gpio_num = LED_GREEN_PIN,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0
        },
        {
            .gpio_num = LED_BLUE_PIN,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_2,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0
        }
    };
    
    for (int i = 0; i < 3; i++) {
        err = ledc_channel_config(&ledc_channel[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel %d configuration failed: %s", i, esp_err_to_name(err));
            return;
        }
        led_pwm_channels[i] = ledc_channel[i].channel;
    }
    
    // Set initial LED state (off)
    set_led_color(0, 0, 0);
    
    ESP_LOGI(TAG, "LEDs initialized");
    #endif
}

/**
 * Initialize sensors
 */
void init_sensors(void) {
    ESP_LOGI(TAG, "Initializing sensors");
    
    #if defined(ARDUINO)
    // Arduino framework - initialize sensor pins
    pinMode(MMWAVE_PRESENCE_PIN, INPUT);
    pinMode(TEMPERATURE_PIN, INPUT);
    
    #else
    // ESP-IDF framework - initialize ADC for temperature sensor
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t err = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit initialization failed: %s", esp_err_to_name(err));
        return;
    }
    
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    err = adc_oneshot_config_channel(adc_handle, TEMPERATURE_PIN, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel configuration failed: %s", esp_err_to_name(err));
        return;
    }
    
    // Initialize mmWave sensor pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MMWAVE_PRESENCE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    #endif
    
    ESP_LOGI(TAG, "Sensors initialized");
}

// ============================================================================
// State Management
// ============================================================================

/**
 * Update node state
 */
void update_state(void) {
    if (!sensors_initialized) return;
    
    // Read sensors
    read_sensors();
    
    // Update timestamp
    node_state.timestamp = millis() / 1000; // Convert to seconds
    
    // Update LED color based on state
    update_led_color();
    
    if (DEBUG_MESH) {
        ESP_LOGD(TAG, "State updated: temp=%d, presence=%d, distance=%dmm",
                 node_state.temperature, node_state.mmwave_presence, node_state.mmwave_distance);
    }
}

/**
 * Read sensor values
 */
void read_sensors(void) {
    if (!sensors_initialized) return;
    
    #if defined(ARDUINO)
    // Arduino framework - read sensors
    
    // Read temperature (simplified - use analog input)
    int temp_raw = analogRead(TEMPERATURE_PIN);
    // Convert to temperature (this is a placeholder - actual conversion depends on sensor)
    node_state.temperature = (int16_t)((temp_raw / 4095.0 * 3.3 - 0.5) * 100.0 * TEMPERATURE_SCALE);
    
    // Read mmWave presence
    node_state.mmwave_presence = digitalRead(MMWAVE_PRESENCE_PIN);
    
    // For now, set distance to 0 (would need actual sensor)
    node_state.mmwave_distance = 0;
    
    #else
    // ESP-IDF framework - read sensors
    
    // Read temperature
    int temp_raw = 0;
    esp_err_t err = adc_oneshot_read(adc_handle, TEMPERATURE_PIN, &temp_raw);
    if (err == ESP_OK) {
        // Convert to temperature (this is a placeholder - actual conversion depends on sensor)
        node_state.temperature = (int16_t)((temp_raw / 4095.0 * 3.3 - 0.5) * 100.0 * TEMPERATURE_SCALE);
    }
    
    // Read mmWave presence
    node_state.mmwave_presence = gpio_get_level(MMWAVE_PRESENCE_PIN);
    
    // For now, set distance to 0 (would need actual sensor)
    node_state.mmwave_distance = 0;
    
    #endif
}

/**
 * Set LED color
 */
void set_led_color(uint8_t r, uint8_t g, uint8_t b) {
    // Store color in node state
    node_state.color[0] = r;
    node_state.color[1] = g;
    node_state.color[2] = b;
    
    #if defined(ARDUINO)
    // Arduino framework - use analogWrite for PWM
    // Note: ESP32 analogWrite uses 8-bit resolution (0-255)
    analogWrite(LED_RED_PIN, r);
    analogWrite(LED_GREEN_PIN, g);
    analogWrite(LED_BLUE_PIN, b);
    
    #else
    // ESP-IDF framework - use LEDC for PWM
    // LEDC uses duty cycle from 0 to (2^resolution - 1)
    uint32_t max_duty = (1 << LEDC_TIMER_8_BIT) - 1;
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, led_pwm_channels[0], (r * max_duty) / 255);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, led_pwm_channels[1], (g * max_duty) / 255);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, led_pwm_channels[2], (b * max_duty) / 255);
    
    ledc_update_duty(LEDC_LOW_SPEED_MODE, led_pwm_channels[0]);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, led_pwm_channels[1]);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, led_pwm_channels[2]);
    
    #endif
}

/**
 * Update LED color based on node state
 */
void update_led_color(void) {
    switch(node_state.state) {
        case NODE_STATE_BOOTING:
            // Blue - booting
            set_led_color(0, 0, 255);
            break;
        case NODE_STATE_ACTIVE:
            // Green - active
            set_led_color(0, 255, 0);
            break;
        case NODE_STATE_ERROR:
            // Red - error
            set_led_color(255, 0, 0);
            break;
        case NODE_STATE_IDLE:
            // Yellow - idle
            set_led_color(255, 255, 0);
            break;
        default:
            // White - unknown
            set_led_color(255, 255, 255);
            break;
    }
}
