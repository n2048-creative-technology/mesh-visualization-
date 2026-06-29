# ESP32C3 Mesh Visualization

Large-scale ESP32C3 mesh firmware with MQTT/WebSocket visualization and activation-kernel LED behavior.

The current implementation uses Espressif ESP-WiFi-Mesh on Seeed XIAO ESP32-C3 boards. Nodes form a self-healing tree mesh, send state upward through the mesh root, and the root publishes MQTT updates for the visualization server. The visualization server also listens for legacy UDP packets and bridges MQTT/UDP updates to the browser via WebSocket.

## Current Behavior

- Nodes join an ESP-WiFi-Mesh network using the router credentials in `firmware/platformio/include/config.h`.
- ESP-WiFi-Mesh elects a root dynamically. The root is the only node that connects to MQTT.
- Non-root nodes send state frames upward to the current mesh root every 5 seconds and whenever meaningful state changes.
- The root republishes every node state on `mesh/state`.
- The root publishes bounded topology summaries on `mesh/topology`.
- Nodes keep trying to reconnect through explicit ESP-WiFi-Mesh self-organization plus a firmware watchdog. Temporary distance, interference, weak power, or router/MQTT outages should recover without manual resets once radio/power conditions improve.
- If NVS is corrupt or stale, firmware recovers the NVS partition; if Wi-Fi returns a zero MAC, the node erases NVS and restarts.
- LED color is computed from the node value/activation state. LED fade changes are included in state JSON but do not trigger extra MQTT publishes by themselves.

## Repository Layout

```text
firmware/platformio/
  platformio.ini          PlatformIO project for Seeed XIAO ESP32-C3
  include/config.h        Mesh, MQTT, sensor, LED, and scaling config
  include/mesh_node.h     Mesh and message data structures
  include/mqtt_handler.h  MQTT publish/command interface
  src/main.cpp            Startup, event handling, main loop, watchdog
  src/mesh_node.cpp       ESP-WiFi-Mesh send/receive and UDP forwarding
  src/mqtt_handler.cpp    MQTT commands, node state, topology JSON
  src/state_manager.cpp   Activation kernels, sensors, LED state

visualization/app/
  server.js               UDP + MQTT ingest, WebSocket bridge, HTTP API
  public/                 Browser visualization

docs/
  architecture.md         Current architecture
  api.md                  MQTT, WebSocket, HTTP, and binary formats
```

## Quick Start

Install the visualization dependencies:

```bash
cd visualization/app
npm install
```

Run Mosquitto on the machine configured as `MQTT_BROKER_IP`, then start the visualization app:

```bash
MQTT_BROKER_URL=mqtt://192.168.178.169:1883 npm start
```

Open:

```text
http://localhost:3000
```

Build firmware:

```bash
cd firmware/platformio
PLATFORMIO_CORE_DIR=/tmp/pio-core PLATFORMIO_BUILD_DIR=/tmp/pio-build-mesh ~/.platformio/penv/bin/pio run
```

Upload to one board:

```bash
PLATFORMIO_CORE_DIR=/tmp/pio-core PLATFORMIO_BUILD_DIR=/tmp/pio-build-mesh ~/.platformio/penv/bin/pio run --target upload --upload-port /dev/ttyACM0
```

Monitor MQTT:

```bash
mosquitto_sub -h 192.168.178.169 -t 'mesh/#' -v
```

## Configuration

Important settings live in `firmware/platformio/include/config.h` and can be overridden with PlatformIO build flags.

| Setting | Current role |
| --- | --- |
| `MESH_ROUTER_SSID`, `MESH_ROUTER_PASS` | Router used by ESP-WiFi-Mesh root |
| `MQTT_BROKER_IP`, `MQTT_BROKER_PORT` | Broker used by the elected root |
| `MESH_MAX_HOPS` | Maximum mesh depth |
| `MESH_AP_CONNECTIONS` | Child capacity per mesh AP |
| `MAX_NEIGHBORS` | Local activation/topology sample size, not total mesh size |
| `MQTT_UPDATE_INTERVAL_MS` | 5-second state heartbeat |
| `MESH_HEALTH_CHECK_INTERVAL_MS` | Watchdog check interval |
| `MESH_RECONNECT_ATTEMPT_MS` | Detached duration before explicitly requesting a mesh reconnect |
| `MESH_RECONNECT_RESTART_MS` | Detached duration before mesh stack restart |
| `MESH_ROOT_IP_RECOVERY_MS` | Root grace period before forcing router-side recovery |
| `MESH_AP_ASSOC_EXPIRE_SECONDS` | Parent keeps a quiet child associated for this many seconds |
| `MQTT_TOPOLOGY_ROUTE_SAMPLE_LIMIT` | Bounded routing-table sample size in MQTT JSON |

The project is shaped for a target of 400-1000 nodes by keeping per-node memory and MQTT topology payloads bounded. `routing_table_size` reports the full mesh count known to the root; `routing_table` and `routing_table_sample` are samples.

## MQTT Topics

### `mesh/state`

Published by the root for itself and for child state frames received through ESP-WiFi-Mesh.

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
  "activations": [],
  "color": [0, 0, 0]
}
```

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
  "routing_table": ["e8:f6:0a:15:f1:c0", "e8:f6:0a:14:38:f0"],
  "routing_table_sample": ["e8:f6:0a:15:f1:c0", "e8:f6:0a:14:38:f0"]
}
```

### `mesh/commands`

The root subscribes to the global command topic and its node-specific command topic. Supported command fields include:

- `preset`
- `kernel`
- `activations`
- `value`
- `reset`

## Visualization Behavior

The Node.js server stores node state in memory, serves `/api/nodes`, `/api/topology`, `/api/stats`, and broadcasts browser updates via WebSocket. MQTT topology references now create placeholder nodes immediately, so a node discovered in the routing table can appear before its first full state frame arrives. Stale nodes are removed after `STALE_NODE_TIMEOUT_MS`, defaulting to 30 seconds.

## Troubleshooting

- If the visualization shows fewer nodes than are powered, check `mesh/topology.routing_table_size` first. The browser is dynamic; a missing powered node usually means that node has not joined the current root's mesh yet, has stale firmware/config, is out of range, or is recovering from a temporary partition.
- If a board appears on USB but not in MQTT, monitor it with PlatformIO. A zero MAC usually indicates NVS/Wi-Fi state corruption; erase flash once and re-upload.
- If nodes reset, lose power, or move, ESP-WiFi-Mesh may re-elect a root and temporarily report smaller routing tables while it heals. Recovered nodes publish immediately on mesh attach/root changes and then continue the 5-second heartbeat.
- PlatformIO may warn that it expected 4 MB but found 2 MB. The current image still builds and boots in the active layout.

## Verification Commands

```bash
node --check visualization/app/server.js
cd firmware/platformio
PLATFORMIO_CORE_DIR=/tmp/pio-core PLATFORMIO_BUILD_DIR=/tmp/pio-build-mesh ~/.platformio/penv/bin/pio run
mosquitto_sub -h 192.168.178.169 -t 'mesh/#' -v
```
