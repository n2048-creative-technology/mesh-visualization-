/**
 * State Manager Header - ESP-IDF Version
 */

#pragma once

#include "config.h"
#include "esp_log.h"

// Function Declarations
void init_state(void);
void init_leds(void);
void init_sensors(void);
void update_state(void);
void read_sensors(void);
void set_led_color(uint8_t r, uint8_t g, uint8_t b);
void update_led_color(void);
void apply_activation_function(void);
void adopt_neighbor_rules(const node_state_t *neighbor_state);
bool load_preset(const char *preset_name);
bool set_kernel_values(const float values[KERNEL_SIZE]);
bool set_activation_rules(const activation_rule_t *rules, uint8_t count);
void reset_state_value(void);
void set_kernel_function(const char *kernel);
void set_activation_function(const char *activation);
const char* get_kernel_function(void);
const char* get_activation_function(void);

// Global Variables
extern bool sensors_initialized;
extern node_state_t node_state;
extern float currentBrightness;
