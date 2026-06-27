/**
 * State Manager Header
 * Manages node state, sensors, and LED control
 * PlatformIO compatible for VS Code
 */

#pragma once

#include "config.h"
#include "esp32_arduino_compat.h"

// ============================================================================
// Function Declarations
// ============================================================================

// Initialization
void init_state(void);
void init_leds(void);
void init_sensors(void);

// State management
void update_state(void);
void read_sensors(void);
void set_led_color(uint8_t r, uint8_t g, uint8_t b);
void update_led_color(void);

// ============================================================================
// Global Variables (extern)
// ============================================================================

extern uint8_t led_pwm_channels[3];
extern bool sensors_initialized;

// Node state variable (defined in mesh_node.cpp)
extern node_state_t node_state;
