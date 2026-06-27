/**
 * State Manager Implementation
 * Manages node state, sensors, and LED control
 */

#include "state_manager.h"
#include "mesh_node.h"
#include <esp_log.h>
#include <driver/ledc.h>
#include <esp_adc/adc_oneshot.h>

static const char *TAG = "STATE_MANAGER";

// Global variables
uint8_t led_pwm_channels[3] = {0};
bool sensors_initialized = false;

// LED PWM configuration
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO      (1 << LED_RED_PIN) | (1 << LED_GREEN_PIN) | (1 << LED_BLUE_PIN)
#define LEDC_CHANNEL_RED    LEDC_CHANNEL_0
#define LEDC_CHANNEL_GREEN  LEDC_CHANNEL_1
#define LEDC_CHANNEL_BLUE   LEDC_CHANNEL_2
#define LEDC_DUTY_RESOLUTION LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY      5000

// ADC configuration for temperature sensor
adc_oneshot_unit_handle_t adc_handle = NULL;

// ============================================================================
// Initialization Functions
// ============================================================================

/**
 * Initialize LED PWM controllers
 */
void init_leds(void) {
    ESP_LOGI(TAG, "Initializing LEDs");
    
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RESOLUTION,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
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
            .speed_mode = LEDC_MODE,
            .channel = LEDC_CHANNEL_RED,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER,
            .duty = 0,
            .hpoint = 0
        },
        {
            .gpio_num = LED_GREEN_PIN,
            .speed_mode = LEDC_MODE,
            .channel = LEDC_CHANNEL_GREEN,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER,
            .duty = 0,
            .hpoint = 0
        },
        {
            .gpio_num = LED_BLUE_PIN,
            .speed_mode = LEDC_MODE,
            .channel = LEDC_CHANNEL_BLUE,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER,
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
}

/**
 * Initialize sensors
 */
void init_sensors(void) {
    ESP_LOGI(TAG, "Initializing sensors");
    
    // Initialize ADC for temperature sensor
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t err = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit initialization failed: %s", esp_err_to_name(err));
        return;
    }
    
    // Configure ADC channel for temperature
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    
    err = adc_oneshot_config_channel(adc_handle, TEMP_SENSOR_PIN, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel configuration failed: %s", esp_err_to_name(err));
        return;
    }
    
    // Initialize mmWave sensor GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MMWAVE_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mmWave sensor GPIO configuration failed: %s", esp_err_to_name(err));
        return;
    }
    
    sensors_initialized = true;
    ESP_LOGI(TAG, "Sensors initialized");
}

/**
 * Initialize state manager
 */
void init_state_manager(void) {
    init_leds();
    init_sensors();
    reset_node_state();
    
    // Indicate boot state
    led_indicate_boot();
    
    ESP_LOGI(TAG, "State manager initialized");
}

// ============================================================================
// LED Control Functions
// ============================================================================

/**
 * Set LED color using RGB values (0-255)
 */
void set_led_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!led_pwm_channels[0]) return; // LEDs not initialized
    
    // Scale 0-255 to 0-255 (8-bit resolution)
    uint32_t duty_red = (r * 255) / 255;
    uint32_t duty_green = (g * 255) / 255;
    uint32_t duty_blue = (b * 255) / 255;
    
    esp_err_t err;
    err = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_RED, duty_red);
    err |= ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_GREEN, duty_green);
    err |= ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_BLUE, duty_blue);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED duty cycles: %s", esp_err_to_name(err));
        return;
    }
    
    // Update duty cycles
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_RED);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_GREEN);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_BLUE);
    
    // Update node state color
    node_state.color[0] = r;
    node_state.color[1] = g;
    node_state.color[2] = b;
}

/**
 * Set LED based on node state
 */
void set_led_state(uint8_t state) {
    switch (state) {
        case NODE_STATE_IDLE:
            set_led_color(0, 0, 255); // Blue
            break;
        case NODE_STATE_ACTIVE:
            set_led_color(0, 255, 0); // Green
            break;
        case NODE_STATE_ERROR:
            set_led_color(255, 0, 0); // Red
            break;
        case NODE_STATE_BOOTING:
            set_led_color(255, 255, 0); // Yellow
            break;
        default:
            set_led_color(0, 0, 0); // Off
            break;
    }
}

/**
 * LED indication for boot state
 */
void led_indicate_boot(void) {
    set_led_state(NODE_STATE_BOOTING);
}

/**
 * LED indication for error state
 */
void led_indicate_error(void) {
    set_led_state(NODE_STATE_ERROR);
}

/**
 * LED indication for active state
 */
void led_indicate_active(void) {
    set_led_state(NODE_STATE_ACTIVE);
}

// ============================================================================
// Sensor Reading Functions
// ============================================================================

/**
 * Read temperature from ADC
 */
void read_temperature(void) {
    if (!sensors_initialized || !adc_handle) {
        node_state.temperature = 0;
        return;
    }
    
    int raw_value = 0;
    esp_err_t err = adc_oneshot_read(adc_handle, TEMP_SENSOR_PIN, &raw_value);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature: %s", esp_err_to_name(err));
        node_state.temperature = 0;
        return;
    }
    
    // Convert ADC reading to temperature
    // This is a placeholder - actual conversion depends on your sensor
    // For ESP32 internal temperature sensor (not accurate):
    // float temp = (raw_value - 500) / 10.0;
    // For external sensor, implement proper calibration
    float temperature_c = (raw_value * 3.3 / 4095.0 - 0.5) * 100.0; // Example conversion
    
    // Store as int16_t * 10
    node_state.temperature = (int16_t)(temperature_c * TEMPERATURE_SCALE);
    
    if (DEBUG_MESH) {
        ESP_LOGD(TAG, "Temperature: %.1f°C (raw: %d)", temperature_c, raw_value);
    }
}

/**
 * Read mmWave sensor
 */
void read_mmwave_sensor(void) {
    if (!sensors_initialized) {
        node_state.mmwave_presence = 0;
        node_state.mmwave_distance = 0;
        return;
    }
    
    // Read presence (digital input)
    node_state.mmwave_presence = gpio_get_level(MMWAVE_SENSOR_PIN);
    
    // For distance, you would typically use I2C/SPI to read from the sensor
    // This is a placeholder - implement based on your actual sensor
    if (node_state.mmwave_presence) {
        // Simulate distance (in production, read from sensor)
        node_state.mmwave_distance = 1500; // 1.5m in mm
    } else {
        node_state.mmwave_distance = 0;
    }
    
    if (DEBUG_MESH) {
        ESP_LOGD(TAG, "mmWave: presence=%d, distance=%d mm", 
                 node_state.mmwave_presence, node_state.mmwave_distance);
    }
}

/**
 * Update all sensors
 */
void update_all_sensors(void) {
    read_temperature();
    read_mmwave_sensor();
}

// ============================================================================
// State Management Functions
// ============================================================================

/**
 * Set node state
 */
void set_node_state(uint8_t state) {
    node_state.state = state;
    set_led_state(state);
    
    if (DEBUG_MESH) {
        ESP_LOGD(TAG, "Node state changed to: %d", state);
    }
}

/**
 * Update node state (timestamp, etc.)
 */
void update_node_state(void) {
    node_state.timestamp = (uint32_t)time(NULL);
    update_all_sensors();
}

/**
 * Reset node state to defaults
 */
void reset_node_state(void) {
    memset(&node_state, 0, sizeof(node_state_t));
    node_state.state = NODE_STATE_BOOTING;
    node_state.color[0] = 255;
    node_state.color[1] = 255;
    node_state.color[2] = 0;
    node_state.temperature = 0;
    node_state.mmwave_presence = 0;
    node_state.mmwave_distance = 0;
    node_state.timestamp = 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get current node state
 */
uint8_t get_node_state(void) {
    return node_state.state;
}

/**
 * Get temperature in Celsius
 */
float get_temperature_c(void) {
    return (float)node_state.temperature / TEMPERATURE_SCALE;
}

/**
 * Get mmWave presence detection
 */
bool get_mmwave_presence(void) {
    return node_state.mmwave_presence != 0;
}

/**
 * Get mmWave distance in mm
 */
uint32_t get_mmwave_distance(void) {
    return node_state.mmwave_distance;
}
