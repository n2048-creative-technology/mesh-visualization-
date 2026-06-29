/**
 * MQTT Handler for Mesh Visualization.
 */

#include "mqtt_handler.h"
#include "config.h"
#include "mesh_node.h"
#include "state_manager.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_mesh.h"

static const char *TAG = "MQTT_HANDLER";

esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

static const char *skip_ws(const char *p) {
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

static bool parse_string_field(const char *json, const char *field, char *out, size_t out_len) {
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", field);

    const char *p = strstr(json, needle);
    if (!p) return false;
    p = strchr(p + strlen(needle), ':');
    if (!p) return false;
    p = skip_ws(p + 1);
    if (*p != '"') return false;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_len) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0;
}

static bool parse_number_field(const char *json, const char *field, float *out) {
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", field);

    const char *p = strstr(json, needle);
    if (!p) return false;
    p = strchr(p + strlen(needle), ':');
    if (!p) return false;
    p = skip_ws(p + 1);

    char *end = NULL;
    float value = strtof(p, &end);
    if (end == p) return false;
    *out = value;
    return true;
}

static bool parse_bool_field(const char *json, const char *field) {
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", field);

    const char *p = strstr(json, needle);
    if (!p) return false;
    p = strchr(p + strlen(needle), ':');
    if (!p) return false;
    p = skip_ws(p + 1);
    return strncmp(p, "true", 4) == 0 || strncmp(p, "1", 1) == 0;
}

static bool parse_float_array_field(const char *json, const char *field, float *values, size_t count) {
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", field);

    const char *p = strstr(json, needle);
    if (!p) return false;
    p = strchr(p + strlen(needle), '[');
    if (!p) return false;
    p++;

    for (size_t i = 0; i < count; ++i) {
        p = skip_ws(p);
        char *end = NULL;
        values[i] = strtof(p, &end);
        if (end == p) return false;
        p = skip_ws(end);
        if (i + 1 < count) {
            if (*p != ',') return false;
            p++;
        }
    }

    return true;
}

static bool parse_activations_field(const char *json, activation_rule_t *rules, uint8_t *count) {
    const char *p = strstr(json, "\"activations\"");
    if (!p) return false;
    p = strchr(p, '[');
    if (!p) return false;
    p++;

    *count = 0;
    while (*p && *p != ']' && *count < MAX_ACTIVATIONS) {
        p = skip_ws(p);
        if (*p == '{') {
            const char *obj_end = strchr(p, '}');
            if (!obj_end) break;

            int op = -1;
            float value = 0.0f;
            const char *op_key = strstr(p, "\"op\"");
            const char *value_key = strstr(p, "\"value\"");
            if (op_key && value_key && op_key < obj_end && value_key < obj_end) {
                op_key = strchr(op_key, ':');
                value_key = strchr(value_key, ':');
                if (op_key && value_key) {
                    op = atoi(skip_ws(op_key + 1));
                    value = strtof(skip_ws(value_key + 1), NULL);
                }
            }

            if (op >= 0 && op <= 4) {
                rules[*count].op = (uint8_t)op;
                rules[*count].value = value;
                (*count)++;
            }
            p = obj_end + 1;
        } else {
            char *end = NULL;
            int op = (int)strtol(p, &end, 10);
            if (end == p) break;
            p = skip_ws(end);
            if (*p != ',') break;
            p = skip_ws(p + 1);
            float value = strtof(p, &end);
            if (end == p) break;
            if (op >= 0 && op <= 4) {
                rules[*count].op = (uint8_t)op;
                rules[*count].value = value;
                (*count)++;
            }
            p = end;
        }

        p = skip_ws(p);
        if (*p == ',') p++;
    }

    return true;
}

static void handle_mqtt_command(const char *payload, int len) {
    char *json = (char *)calloc((size_t)len + 1, 1);
    if (!json) return;
    memcpy(json, payload, len);

    char preset[32];
    if (parse_string_field(json, "preset", preset, sizeof(preset))) {
        load_preset(preset);
    }

    float kernel[KERNEL_SIZE];
    if (parse_float_array_field(json, "kernel", kernel, KERNEL_SIZE)) {
        set_kernel_values(kernel);
    }

    activation_rule_t rules[MAX_ACTIVATIONS] = {};
    uint8_t count = 0;
    if (parse_activations_field(json, rules, &count)) {
        set_activation_rules(rules, count);
    }

    float value = 0.0f;
    if (parse_number_field(json, "value", &value)) {
        node_state.value = value != 0.0f ? 1 : 0;
        node_state.value_sequence++;
    }

    if (parse_bool_field(json, "reset")) {
        reset_state_value();
    }

    free(json);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_connected = true;
            esp_mqtt_client_subscribe(mqtt_client, MQTT_COMMAND_TOPIC, 0);
            {
                char node_topic[64];
                char mac_str[18];
                snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(node_mac));
                snprintf(node_topic, sizeof(node_topic), "%s/%s", MQTT_COMMAND_TOPIC, mac_str);
                esp_mqtt_client_subscribe(mqtt_client, node_topic, 0);
            }
            mqtt_publish_topology_and_state();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_connected = false;
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            handle_mqtt_command(event->data, event->data_len);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            mqtt_connected = false;
            break;

        default:
            ESP_LOGD(TAG, "MQTT event: %d", event->event_id);
            break;
    }
}

void init_mqtt(void) {
    if (mqtt_client != NULL) {
        return;
    }

    static char client_id[40];
    static char broker_uri[64];
    snprintf(client_id, sizeof(client_id), "esp32_mesh_%02X%02X%02X",
             node_mac[3], node_mac[4], node_mac[5]);
    snprintf(broker_uri, sizeof(broker_uri), "mqtt://%s", MQTT_BROKER_IP);

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = broker_uri;
    mqtt_cfg.broker.address.port = MQTT_BROKER_PORT;
    mqtt_cfg.credentials.client_id = client_id;
    mqtt_cfg.task.priority = 5;
    mqtt_cfg.task.stack_size = 6144;
    mqtt_cfg.buffer.size = 4096;
    mqtt_cfg.network.timeout_ms = 10000;
    mqtt_cfg.network.reconnect_timeout_ms = 10000;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        mqtt_client = NULL;
        return;
    }

    ESP_LOGI(TAG, "MQTT client started: %s", broker_uri);
}

bool is_mqtt_connected(void) {
    return mqtt_connected;
}

void mqtt_disconnect(void) {
    if (mqtt_client != NULL) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        mqtt_connected = false;
    }
}

static void format_kernel_json(const node_state_t *state, char *kernel, size_t kernel_len) {
    size_t offset = 0;
    offset += snprintf(kernel + offset, kernel_len - offset, "[");
    for (int i = 0; i < KERNEL_SIZE && offset < kernel_len; ++i) {
        offset += snprintf(kernel + offset, kernel_len - offset, "%s%.4f",
                           i == 0 ? "" : ",", state->kernel[i]);
    }
    if (offset < kernel_len) {
        snprintf(kernel + offset, kernel_len - offset, "]");
    }
}

static void format_activations_json(const node_state_t *state, char *activations, size_t activations_len) {
    size_t offset = 0;
    offset += snprintf(activations + offset, activations_len - offset, "[");
    for (uint8_t i = 0; i < state->activation_count && offset < activations_len; ++i) {
        offset += snprintf(activations + offset, activations_len - offset,
                           "%s{\"op\":%u,\"value\":%.4f}",
                           i == 0 ? "" : ",",
                           state->activations[i].op,
                           state->activations[i].value);
    }
    if (offset < activations_len) {
        snprintf(activations + offset, activations_len - offset, "]");
    }
}

void mqtt_publish_node_state(const uint8_t *node_mac_value, const node_state_t *state) {
    if (!mqtt_connected || mqtt_client == NULL) {
        return;
    }
    if (node_mac_value == NULL || state == NULL) {
        return;
    }

    char mac[18];
    char *kernel = (char *)calloc(192, 1);
    char *activations = (char *)calloc(256, 1);
    char *payload = (char *)calloc(1024, 1);
    if (kernel == NULL || activations == NULL || payload == NULL) {
        ESP_LOGW(TAG, "Skipping MQTT state publish: allocation failed");
        free(kernel);
        free(activations);
        free(payload);
        return;
    }

    snprintf(mac, sizeof(mac), MACSTR, MAC2STR(node_mac_value));
    format_kernel_json(state, kernel, 192);
    format_activations_json(state, activations, 256);

    snprintf(payload, 1024,
             "{\"mac\":\"%s\",\"state\":%u,\"temperature\":%d,"
             "\"mmwave_presence\":%u,\"mmwave_distance\":%lu,\"timestamp\":%lu,"
             "\"value\":%u,\"activation_sum\":%.4f,"
             "\"kernel_function\":\"%s\",\"activation_function\":\"%s\","
             "\"kernel_sequence\":%lu,\"value_sequence\":%lu,"
             "\"activation_sequence\":%lu,\"activation_count\":%u,"
             "\"kernel\":%s,\"activations\":%s,"
             "\"color\":[%u,%u,%u]}",
             mac, state->state, state->temperature,
             state->mmwave_presence, (unsigned long)state->mmwave_distance,
             (unsigned long)state->timestamp,
             state->value, state->activation_sum,
             get_kernel_function(), get_activation_function(),
             (unsigned long)state->kernel_sequence,
             (unsigned long)state->value_sequence,
             (unsigned long)state->activation_sequence,
             state->activation_count, kernel, activations,
             state->color[0], state->color[1], state->color[2]);

    esp_mqtt_client_publish(mqtt_client, MQTT_STATE_TOPIC, payload, 0, 0, 0);
    free(kernel);
    free(activations);
    free(payload);
}

void mqtt_publish_state(void) {
    mqtt_publish_node_state(node_mac, &node_state);
}

void mqtt_publish_topology_and_state(void) {
    if (is_mqtt_connected()) {
        mqtt_publish_topology();
        mqtt_publish_state();
    }
}

void mqtt_publish_topology(void) {
    if (!mqtt_connected || mqtt_client == NULL) {
        return;
    }

    char mac[18];
    char *neighbors = (char *)calloc(512, 1);
    char *routing = (char *)calloc(640, 1);
    char *payload = (char *)calloc(2400, 1);
    if (neighbors == NULL || routing == NULL || payload == NULL) {
        ESP_LOGW(TAG, "Skipping MQTT topology publish: allocation failed");
        free(neighbors);
        free(routing);
        free(payload);
        return;
    }

    snprintf(mac, sizeof(mac), MACSTR, MAC2STR(node_mac));

    size_t offset = 0;
    offset += snprintf(neighbors + offset, 512 - offset, "[");
    bool first = true;
    int neighbor_sample_count = 0;
    for (int i = 0; i < MAX_NEIGHBORS &&
                    neighbor_sample_count < MQTT_TOPOLOGY_NEIGHBOR_SAMPLE_LIMIT &&
                    offset < 512; i++) {
        if (!neighbor_list[i].active) continue;

        char neighbor_mac[18];
        snprintf(neighbor_mac, sizeof(neighbor_mac), MACSTR, MAC2STR(neighbor_list[i].mac));
        offset += snprintf(neighbors + offset, 512 - offset,
                           "%s{\"mac\":\"%s\",\"rssi\":%d,\"last_seen\":%lu,\"value\":%u}",
                           first ? "" : ",", neighbor_mac, neighbor_list[i].rssi,
                           (unsigned long)neighbor_list[i].last_seen,
                           neighbor_list[i].has_state ? neighbor_list[i].state.value : 0);
        first = false;
        neighbor_sample_count++;
    }
    if (offset < 512) {
        snprintf(neighbors + offset, 512 - offset, "]");
    }

    int layer = mesh_initialized ? esp_mesh_get_layer() : 0;
    int rtable_size = mesh_initialized ? esp_mesh_get_routing_table_size() : 0;
    int route_count = 0;
    mesh_addr_t route_table[MQTT_TOPOLOGY_ROUTE_SAMPLE_LIMIT] = {};

    if (mesh_initialized && rtable_size > 0) {
        int requested = MIN(rtable_size, (int)(sizeof(route_table) / sizeof(route_table[0])));
        esp_err_t err = esp_mesh_get_routing_table(route_table,
                                                   requested * sizeof(mesh_addr_t),
                                                   &route_count);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read mesh routing table: %s", esp_err_to_name(err));
            route_count = 0;
        }
    }

    offset = 0;
    offset += snprintf(routing + offset, 640 - offset, "[");
    for (int i = 0; i < route_count && offset < 640; i++) {
        char route_mac[18];
        snprintf(route_mac, sizeof(route_mac), MACSTR, MAC2STR(route_table[i].addr));
        offset += snprintf(routing + offset, 640 - offset,
                           "%s\"%s\"", i == 0 ? "" : ",", route_mac);
    }
    if (offset < 640) {
        snprintf(routing + offset, 640 - offset, "]");
    }

    snprintf(payload, 2400,
             "{\"node_mac\":\"%s\",\"node_state\":%u,\"value\":%u,"
             "\"layer\":%d,\"target_node_count\":%d,"
             "\"routing_table_size\":%d,\"route_sample_count\":%d,"
             "\"routing_table_truncated\":%s,"
             "\"neighbor_count\":%d,\"neighbor_sample_count\":%d,"
             "\"neighbors\":%s,\"routing_table\":%s,\"routing_table_sample\":%s}",
             mac, node_state.state, node_state.value,
             layer, MESH_TARGET_NODE_COUNT,
             rtable_size, route_count,
             route_count < rtable_size ? "true" : "false",
             get_neighbor_count(), neighbor_sample_count,
             neighbors, routing, routing);

    esp_mqtt_client_publish(mqtt_client, MQTT_TOPOLOGY_TOPIC, payload, 0, 0, 0);
    free(neighbors);
    free(routing);
    free(payload);
}
