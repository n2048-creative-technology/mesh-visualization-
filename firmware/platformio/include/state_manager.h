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

// Global Variables
extern uint8_t led_pwm_channels[3];
extern bool sensors_initialized;
extern node_state_t node_state;
