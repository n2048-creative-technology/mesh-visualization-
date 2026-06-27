/**
 * State Manager Header
 * Manages node state, sensors, and LED control
 * PlatformIO compatible for VS Code
 */

#pragma once

#include "config.h"
#include "mesh_node.h"

// ============================================================================
// State Definitions
// ============================================================================

#define NODE_STATE_IDLE 0
#define NODE_STATE_ACTIVE 1
#define NODE_STATE_ERROR 2
#define NODE_STATE_BOOTING 3

// ============================================================================
// Function Declarations
// ============================================================================

// Initialization
void init_leds(void);
void init_sensors(void);
void init_state_manager(void);

// LED Control
void set_led_color(uint8_t r, uint8_t g, uint8_t b);
void set_led_state(uint8_t state);
void led_indicate_boot(void);
void led_indicate_error(void);
void led_indicate_active(void);

// Sensor Reading
void read_temperature(void);
void read_mmwave_sensor(void);
void update_all_sensors(void);

// State Management
void set_node_state(uint8_t state);
void update_node_state(void);
void reset_node_state(void);

// Utility
uint8_t get_node_state(void);
float get_temperature_c(void);
bool get_mmwave_presence(void);
uint32_t get_mmwave_distance(void);

// ============================================================================
// Global Variables (extern)
// ============================================================================

extern uint8_t led_pwm_channels[3];
extern bool sensors_initialized;
