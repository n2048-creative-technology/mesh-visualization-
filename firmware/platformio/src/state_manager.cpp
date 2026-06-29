/**
 * State Manager Implementation - ESP-IDF Version
 * Cellular automata kernel/activation processing based on emergent-esp32.
 */

#include "state_manager.h"
#include "mesh_node.h"

#include <math.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

static const char *TAG = "STATE_MANAGER";

bool sensors_initialized = false;
node_state_t node_state;
float currentBrightness = 0.0f;

static uint32_t lastTempReadMs = 0;
static led_strip_handle_t led_strip = NULL;

static char kernel_function[64] = "random";
static char activation_function[128] = "threshold";

static const float kKernelConway[KERNEL_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 9};
static const activation_rule_t kActivationsConway[] = {{2, 3.0f}, {2, 11.0f}, {2, 12.0f}};
static const float kKernelRule30[KERNEL_SIZE] = {1, 1, 1, 0, 0, 0, 0, 0, 0};
static const activation_rule_t kActivationsRule30[] = {{2, 1.0f}, {2, 2.0f}};
static const float kKernelMajority[KERNEL_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 0};
static const activation_rule_t kActivationsMajority[] = {{3, 4.5f}};
static const float kKernelAnd[KERNEL_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 0};
static const activation_rule_t kActivationsAnd[] = {{2, 8.0f}};
static const float kKernelOr[KERNEL_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 0};
static const activation_rule_t kActivationsOr[] = {{3, 0.5f}};

static void describe_activation_rules(void) {
    char buffer[sizeof(activation_function)];
    size_t offset = 0;

    if (node_state.activation_count == 0) {
        strlcpy(activation_function, "threshold", sizeof(activation_function));
        return;
    }

    for (uint8_t i = 0; i < node_state.activation_count && offset < sizeof(buffer); ++i) {
        const char *op = "?";
        switch (node_state.activations[i].op) {
            case 0: op = "<"; break;
            case 1: op = "<="; break;
            case 2: op = "=="; break;
            case 3: op = ">="; break;
            case 4: op = ">"; break;
        }
        int written = snprintf(buffer + offset, sizeof(buffer) - offset, "%s%s%.2f",
                               i == 0 ? "" : "|", op, node_state.activations[i].value);
        if (written < 0) break;
        offset += (size_t)written;
    }

    buffer[sizeof(buffer) - 1] = '\0';
    strlcpy(activation_function, buffer, sizeof(activation_function));
}

static bool load_preset_data(const char *preset_name, const float **kernel,
                             const activation_rule_t **rules, uint8_t *count) {
    if (strcmp(preset_name, "conway") == 0) {
        *kernel = kKernelConway;
        *rules = kActivationsConway;
        *count = sizeof(kActivationsConway) / sizeof(kActivationsConway[0]);
        return true;
    }
    if (strcmp(preset_name, "rule30") == 0) {
        *kernel = kKernelRule30;
        *rules = kActivationsRule30;
        *count = sizeof(kActivationsRule30) / sizeof(kActivationsRule30[0]);
        return true;
    }
    if (strcmp(preset_name, "majority") == 0) {
        *kernel = kKernelMajority;
        *rules = kActivationsMajority;
        *count = sizeof(kActivationsMajority) / sizeof(kActivationsMajority[0]);
        return true;
    }
    if (strcmp(preset_name, "and") == 0) {
        *kernel = kKernelAnd;
        *rules = kActivationsAnd;
        *count = sizeof(kActivationsAnd) / sizeof(kActivationsAnd[0]);
        return true;
    }
    if (strcmp(preset_name, "or") == 0) {
        *kernel = kKernelOr;
        *rules = kActivationsOr;
        *count = sizeof(kActivationsOr) / sizeof(kActivationsOr[0]);
        return true;
    }
    return false;
}

void init_sensors(void) {
    gpio_config_t mmwave_cfg = {};
    mmwave_cfg.pin_bit_mask = 1ULL << MMWAVE_PRESENCE_PIN;
    mmwave_cfg.mode = GPIO_MODE_INPUT;
    mmwave_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    mmwave_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    mmwave_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&mmwave_cfg);

    sensors_initialized = true;
    ESP_LOGI(TAG, "Sensors initialized");
}

float read_temperature(void) {
    static float simulated_temp = 25.0f;
    static uint32_t last_temp_update = 0;
    uint32_t now = esp_timer_get_time() / 1000;

    if (now - last_temp_update > 10000) {
        last_temp_update = now;
        simulated_temp += 0.5f;
        if (simulated_temp > 30.0f) simulated_temp = 25.0f;
    }

    return simulated_temp;
}

bool read_mmwave_presence(void) {
    return gpio_get_level((gpio_num_t)MMWAVE_PRESENCE_PIN) == 1;
}

uint32_t read_mmwave_distance(void) {
    return 0;
}

void read_sensors(void) {
    if (!sensors_initialized) return;

    uint32_t now = esp_timer_get_time() / 1000;

    if (now - lastTempReadMs >= TEMP_READ_INTERVAL_MS) {
        lastTempReadMs = now;
        node_state.temperature = (int16_t)(read_temperature() * TEMPERATURE_SCALE);
    }

    node_state.mmwave_presence = read_mmwave_presence() ? 1 : 0;
    node_state.mmwave_distance = read_mmwave_distance();
    node_state.timestamp = now;
}

void init_leds(void) {
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = DATA_PIN;
    strip_config.max_leds = NUM_LEDS;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.flags.invert_out = false;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = LED_STRIP_RMT_RES_HZ;

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LED strip init failed: %s", esp_err_to_name(err));
        led_strip = NULL;
        return;
    }

    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "WS2812 LED initialized on GPIO %d", DATA_PIN);
}

void set_led_color(uint8_t r, uint8_t g, uint8_t b) {
    node_state.color[0] = r;
    node_state.color[1] = g;
    node_state.color[2] = b;

    if (led_strip) {
        led_strip_set_pixel(led_strip, 0, r, g, b);
        led_strip_refresh(led_strip);
    }
}

void update_led_color(void) {
    float targetBrightness = node_state.value ? 255.0f : 0.0f;
    currentBrightness += (targetBrightness - currentBrightness) * TRANSITION_SPEED;
    currentBrightness = CLAMP(currentBrightness, 0.0f, 255.0f);

    uint8_t smoothed = (uint8_t)currentBrightness;
    set_led_color(smoothed, smoothed, smoothed);
}

bool set_kernel_values(const float values[KERNEL_SIZE]) {
    memcpy(node_state.kernel, values, sizeof(node_state.kernel));
    node_state.kernel_sequence++;
    strlcpy(kernel_function, "custom", sizeof(kernel_function));
    ESP_LOGI(TAG, "Kernel updated, kseq=%lu", (unsigned long)node_state.kernel_sequence);
    return true;
}

bool set_activation_rules(const activation_rule_t *rules, uint8_t count) {
    if (count > MAX_ACTIVATIONS) return false;

    memset(node_state.activations, 0, sizeof(node_state.activations));
    if (count > 0 && rules != NULL) {
        memcpy(node_state.activations, rules, count * sizeof(activation_rule_t));
    }
    node_state.activation_count = count;
    node_state.activation_sequence++;
    describe_activation_rules();
    ESP_LOGI(TAG, "Activations updated, count=%u aseq=%lu",
             node_state.activation_count, (unsigned long)node_state.activation_sequence);
    return true;
}

bool load_preset(const char *preset_name) {
    const float *kernel = NULL;
    const activation_rule_t *rules = NULL;
    uint8_t count = 0;

    if (!load_preset_data(preset_name, &kernel, &rules, &count)) {
        ESP_LOGW(TAG, "Unknown preset: %s", preset_name);
        return false;
    }

    memcpy(node_state.kernel, kernel, sizeof(node_state.kernel));
    memset(node_state.activations, 0, sizeof(node_state.activations));
    memcpy(node_state.activations, rules, count * sizeof(activation_rule_t));
    node_state.activation_count = count;
    node_state.kernel_sequence++;
    node_state.activation_sequence++;
    strlcpy(kernel_function, preset_name, sizeof(kernel_function));
    describe_activation_rules();
    ESP_LOGI(TAG, "Loaded preset '%s' kseq=%lu aseq=%lu", preset_name,
             (unsigned long)node_state.kernel_sequence,
             (unsigned long)node_state.activation_sequence);
    return true;
}

void reset_state_value(void) {
    node_state.value = esp_random() % 2;
    node_state.value_sequence++;
    node_state.state = node_state.value ? NODE_STATE_ACTIVE : NODE_STATE_IDLE;
    ESP_LOGI(TAG, "Value reset to %u, vseq=%lu",
             node_state.value, (unsigned long)node_state.value_sequence);
}

void adopt_neighbor_rules(const node_state_t *neighbor_state) {
    if (neighbor_state == NULL) return;

    if (neighbor_state->kernel_sequence > node_state.kernel_sequence) {
        memcpy(node_state.kernel, neighbor_state->kernel, sizeof(node_state.kernel));
        node_state.kernel_sequence = neighbor_state->kernel_sequence;
        strlcpy(kernel_function, "mesh", sizeof(kernel_function));
        ESP_LOGI(TAG, "Adopted newer kernel from mesh, kseq=%lu",
                 (unsigned long)node_state.kernel_sequence);
    }

    if (neighbor_state->activation_sequence > node_state.activation_sequence) {
        memcpy(node_state.activations, neighbor_state->activations, sizeof(node_state.activations));
        node_state.activation_count = MIN(neighbor_state->activation_count, MAX_ACTIVATIONS);
        node_state.activation_sequence = neighbor_state->activation_sequence;
        describe_activation_rules();
        ESP_LOGI(TAG, "Adopted newer activations from mesh, aseq=%lu",
                 (unsigned long)node_state.activation_sequence);
    }

    if (neighbor_state->value_sequence > node_state.value_sequence) {
        node_state.value_sequence = neighbor_state->value_sequence;
        node_state.value = esp_random() % 2;
        ESP_LOGI(TAG, "Adopted value reset from mesh, value=%u vseq=%lu",
                 node_state.value, (unsigned long)node_state.value_sequence);
    }
}

void apply_activation_function(void) {
    float inputs[KERNEL_SIZE] = {};
    fill_neighbor_values(inputs, KERNEL_SIZE - 1);
    if (node_state.mmwave_presence) {
        inputs[0] = 1.0f;
    }
    inputs[KERNEL_SIZE - 1] = (float)node_state.value;

    float sum = 0.0f;
    for (size_t i = 0; i < KERNEL_SIZE; ++i) {
        sum += node_state.kernel[i] * inputs[i];
    }
    node_state.activation_sum = sum;

    bool nextValue = false;
    if (node_state.activation_count > 0) {
        for (uint8_t i = 0; i < node_state.activation_count; ++i) {
            const activation_rule_t *rule = &node_state.activations[i];
            switch (rule->op) {
                case 0: nextValue = sum < rule->value; break;
                case 1: nextValue = sum <= rule->value; break;
                case 2: nextValue = fabsf(sum - rule->value) < 0.0001f; break;
                case 3: nextValue = sum >= rule->value; break;
                case 4: nextValue = sum > rule->value; break;
                default: nextValue = false; break;
            }
            if (nextValue) break;
        }
    } else {
        nextValue = sum > 0.0f;
    }

    node_state.value = nextValue ? 1 : 0;
    node_state.state = node_state.value ? NODE_STATE_ACTIVE : NODE_STATE_IDLE;

    float temp = node_state.temperature / TEMPERATURE_SCALE;
    if (temp > 80.0f) {
        node_state.state = NODE_STATE_ERROR;
        ESP_LOGW(TAG, "High temperature detected: %.1f C", temp);
    }
}

void init_state(void) {
    memset(&node_state, 0, sizeof(node_state_t));
    node_state.state = NODE_STATE_BOOTING;
    node_state.value = esp_random() % 2;

    for (size_t i = 0; i < KERNEL_SIZE; ++i) {
        node_state.kernel[i] = ((int32_t)(esp_random() % 20001) - 10000) / 10000.0f;
    }

    init_leds();
    init_sensors();
    update_led_color();
    node_state.state = node_state.value ? NODE_STATE_ACTIVE : NODE_STATE_IDLE;
    ESP_LOGI(TAG, "State manager initialized value=%u kernel=random", node_state.value);
}

void update_state(void) {
    if (!sensors_initialized) return;
    read_sensors();
    apply_activation_function();
    node_state.timestamp = esp_timer_get_time() / 1000;
}

void set_kernel_function(const char *kernel) {
    if (kernel == NULL || kernel[0] == '\0') return;
    if (!load_preset(kernel)) {
        strlcpy(kernel_function, kernel, sizeof(kernel_function));
    }
}

void set_activation_function(const char *activation) {
    if (activation == NULL || activation[0] == '\0') return;
    strlcpy(activation_function, activation, sizeof(activation_function));
}

const char* get_kernel_function(void) {
    return kernel_function;
}

const char* get_activation_function(void) {
    return activation_function;
}
