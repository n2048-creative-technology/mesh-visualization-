/**
 * MQTT Handler for Mesh Visualization
 * ESP-IDF Framework Implementation
 */

#pragma once

#include "config.h"
#include "mqtt_client.h"

// MQTT Client Handle
extern esp_mqtt_client_handle_t mqtt_client;

// Function Declarations
void init_mqtt(void);
void mqtt_publish_topology(void);
void mqtt_publish_state(void);
void mqtt_publish_node_state(const uint8_t *mac, const node_state_t *state);
void mqtt_publish_topology_and_state(void);
void mqtt_disconnect(void);
bool is_mqtt_connected(void);
