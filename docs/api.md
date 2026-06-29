# API and Message Formats

This document describes the current wire formats exposed by the firmware and visualization server.

## MQTT

The elected ESP-WiFi-Mesh root is the MQTT client. Child nodes send state through ESP-WiFi-Mesh; the root republishes those child states on MQTT.

Default broker settings are in `firmware/platformio/include/config.h`:

- `MQTT_BROKER_IP`
- `MQTT_BROKER_PORT`
- `MQTT_TOPOLOGY_TOPIC`
- `MQTT_STATE_TOPIC`
- `MQTT_COMMAND_TOPIC`

### `mesh/state`

Published for root and child node states.

```json
{
  "mac": "e8:f6:0a:14:38:f0",
  "state": 1,
  "temperature": 270,
  "mmwave_presence": 0,
  "mmwave_distance": 0,
  "timestamp": 45259,
  "value": 0,
  "activation_sum": 0.0,
  "kernel_function": "random",
  "activation_function": "threshold",
  "kernel_sequence": 0,
  "value_sequence": 0,
  "activation_sequence": 0,
  "activation_count": 0,
  "kernel": [-0.4134, 0.2254, 0.5180, -0.2368, 0.4129, -0.2266, 0.3738, -0.8186, -0.7269],
  "activations": [
    {"op": 3, "value": 0.5}
  ],
  "color": [0, 0, 0]
}
```

Notes:

- `temperature` is temperature multiplied by 10.
- `value` is the binary activation/LED value.
- `kernel` has `KERNEL_SIZE` entries, currently 9.
- `activations` has up to `MAX_ACTIVATIONS` entries.
- LED fade color is included, but color-only fade updates do not trigger extra state publishes.

### `mesh/topology`

Published periodically by the root.

```json
{
  "node_mac": "e8:f6:0a:15:f1:c0",
  "node_state": 1,
  "value": 0,
  "layer": 1,
  "target_node_count": 1000,
  "routing_table_size": 5,
  "route_sample_count": 5,
  "routing_table_truncated": false,
  "neighbor_count": 2,
  "neighbor_sample_count": 2,
  "neighbors": [
    {"mac": "e8:f6:0a:16:02:38", "rssi": -50, "last_seen": 9718, "value": 0}
  ],
  "routing_table": [
    "e8:f6:0a:15:f1:c0",
    "e8:f6:0a:14:38:f0"
  ],
  "routing_table_sample": [
    "e8:f6:0a:15:f1:c0",
    "e8:f6:0a:14:38:f0"
  ]
}
```

`routing_table_size` is the full count reported by ESP-WiFi-Mesh. `routing_table` and `routing_table_sample` are bounded samples, not guaranteed full lists when `routing_table_truncated` is true.

### `mesh/commands`

The root subscribes to:

- `mesh/commands`
- `mesh/commands/<root-mac>`

Supported JSON fields:

```json
{
  "preset": "random",
  "kernel": [0.1, -0.2, 0.3, 0.4, -0.5, 0.6, -0.7, 0.8, 0.9],
  "activations": [
    {"op": 3, "value": 0.5}
  ],
  "value": 1,
  "reset": false
}
```

Activation op values:

| op | Meaning |
| --- | --- |
| 0 | `<` |
| 1 | `<=` |
| 2 | `==` |
| 3 | `>=` |
| 4 | `>` |

## Internal ESP-WiFi-Mesh Message

The internal mesh frame is `mesh_message_t` from `firmware/platformio/include/mesh_node.h`.

```c
typedef struct {
    uint8_t version;
    uint8_t msg_type;
    uint8_t mac[6];
    node_state_t state;
    uint32_t sequence;
    uint32_t mesh_timestamp;
    neighbor_info_t neighbors[MAX_NEIGHBORS];
    uint16_t checksum;
} mesh_message_t;
```

Checksum is the byte sum up to `offsetof(mesh_message_t, checksum)`, so trailing struct padding is excluded.

## Legacy UDP Message

The visualization server still accepts a compact binary UDP message on port `1234`.

```c
#pragma pack(push, 1)
typedef struct {
    uint8_t version;
    uint8_t msg_type;
    uint8_t mac[6];
    uint8_t state;
    uint8_t color[3];
    int16_t temperature;
    uint8_t mmwave_presence;
    uint32_t mmwave_distance;
    uint32_t timestamp;
    struct {
        uint8_t mac[6];
        int8_t rssi;
    } neighbors[8];
    uint16_t checksum;
} udp_message_t;
#pragma pack(pop)
```

UDP checksum is the byte sum up to `offsetof(udp_message_t, checksum)`.

## State Codes

| Code | Name | Meaning |
| --- | --- | --- |
| 0 | `NODE_STATE_BOOTING` | Booting |
| 1 | `NODE_STATE_IDLE` | Idle |
| 2 | `NODE_STATE_ACTIVE` | Active |
| 3 | `NODE_STATE_ERROR` | Error |

## HTTP API

Base URL:

```text
http://<server>:3000/api
```

### `GET /api/nodes`

Returns all nodes known by the visualization server.

```json
[
  {
    "version": 1,
    "msgType": 1,
    "mac": "e8:f6:0a:14:38:f0",
    "state": 1,
    "color": {"r": 0, "g": 0, "b": 0},
    "temperature": 27.0,
    "mmwavePresence": false,
    "mmwaveDistance": 0,
    "timestamp": 45259,
    "neighbors": [
      {"mac": "e8:f6:0a:15:f1:c0", "rssi": -127}
    ],
    "receivedAt": 1719500001000,
    "discoveredFromTopology": false
  }
]
```

Nodes discovered from topology before their first state message are returned with `discoveredFromTopology: true`, gray color, zero temperature, and placeholder neighbor data.

### `GET /api/topology`

Returns a map keyed by MAC address.

```json
{
  "e8:f6:0a:14:38:f0": {
    "neighbors": ["e8:f6:0a:15:f1:c0"],
    "rssi": [-127],
    "state": 1,
    "color": {"r": 128, "g": 128, "b": 128},
    "temperature": 0,
    "mmwavePresence": false,
    "mmwaveDistance": 0
  }
}
```

### `GET /api/stats`

```json
{
  "nodeCount": 5,
  "messageCount": 12345,
  "lastMessageTime": 1719500001000,
  "uptime": 3600.5
}
```

### `GET /api/mqtt`

Returns MQTT connection status and topic configuration.

## WebSocket

Browser clients connect to:

```text
ws://<server>:3000
```

Server-to-client message types:

### `full_state`

Sent on connection.

```json
{
  "type": "full_state",
  "data": []
}
```

### `node_update`

Sent whenever a node changes or a topology placeholder is created.

```json
{
  "type": "node_update",
  "data": {
    "mac": "e8:f6:0a:14:38:f0",
    "state": 1,
    "color": {"r": 0, "g": 0, "b": 0},
    "temperature": 27.0,
    "neighbors": []
  }
}
```

### `stats`

Sent every 5 seconds.

```json
{
  "type": "stats",
  "data": {
    "nodeCount": 5,
    "messageCount": 12345,
    "lastMessageTime": 1719500001000
  }
}
```

## Timing and Cleanup

- Firmware updates sensors and LED state every `STATE_UPDATE_INTERVAL`.
- Firmware publishes/sends state every `MQTT_UPDATE_INTERVAL_MS` and on meaningful state changes.
- Visualization stale-node cleanup defaults to 30 seconds and can be configured with `STALE_NODE_TIMEOUT_MS`.
- Mesh health check defaults to 10 seconds.
- Mesh stack restart after detachment defaults to 60 seconds.

## Compatibility

- Current target: Seeed XIAO ESP32-C3 with ESP-IDF through PlatformIO.
- Visualization server: Node.js with MQTT and WebSocket support.
- MQTT broker: Mosquitto or compatible MQTT 3 broker.
