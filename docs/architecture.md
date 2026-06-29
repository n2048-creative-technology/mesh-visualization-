# Architecture

This project combines ESP-WiFi-Mesh firmware, MQTT state publishing, and a browser visualization. The current implementation is root-forwarded: all ESP32 nodes form a mesh, non-root nodes send state frames upward, and the elected root bridges the mesh to MQTT and UDP visualization output.

## Runtime Topology

```text
ESP32-C3 nodes
  |
  | ESP-WiFi-Mesh tree, dynamic root election, self-healing
  v
Elected mesh root
  |
  | MQTT: mesh/state, mesh/topology
  | UDP: legacy visualization frame forwarding
  v
Mosquitto + Node.js visualization server
  |
  | WebSocket + HTTP API
  v
Browser D3 visualization
```

ESP-WiFi-Mesh chooses the root. The root can change when devices reboot, disappear, or reparent. The visualization should treat the root as an implementation detail, not as a fixed gateway identity.

## Firmware Components

### `main.cpp`

- Initializes NVS with recovery for `ESP_ERR_NVS_NO_FREE_PAGES` and `ESP_ERR_NVS_NEW_VERSION_FOUND`.
- Guards against zero Wi-Fi MAC by erasing NVS and restarting.
- Initializes Wi-Fi, ESP-WiFi-Mesh, state management, UDP, TCP command handling, and MQTT on root nodes.
- Handles mesh events such as parent connect/disconnect, root address changes, layer changes, and root DHCP startup.
- Runs the main loop:
  - Updates sensors and LED state every `STATE_UPDATE_INTERVAL`.
  - Publishes/sends state on meaningful state changes and every `MQTT_UPDATE_INTERVAL_MS`.
  - Requests an immediate state/topology publish after mesh attach, root address, child attach/detach, routing-table, and IP events.
  - Runs a mesh health watchdog.

### `mesh_node.cpp`

- Configures ESP-WiFi-Mesh in tree topology.
- Keeps a bounded local `neighbor_list[MAX_NEIGHBORS]`.
- Receives mesh state frames.
- On root:
  - Publishes received child state to MQTT.
  - Forwards received child state to the UDP visualization path.
- On non-root:
  - Sends state upward to the cached root address when available.
  - Falls back to ESP-WiFi-Mesh ToDS send mode when root address is not known.
- Uses checksum calculation up to the checksum field offset, avoiding trailing struct padding.

### `state_manager.cpp`

- Maintains node state, LED color, temperature, mmWave presence, binary value, activation sum, kernel values, and activation rules.
- Applies activation/kernel behavior inspired by `n2048-creative-technology/emergent-esp32`.
- Adopts newer kernel/value/activation sequences received from neighbors.

### `mqtt_handler.cpp`

- Runs only on the elected root.
- Publishes `mesh/state` for root and child nodes.
- Publishes `mesh/topology` as a bounded summary.
- Subscribes to `mesh/commands` and `mesh/commands/<root-mac>`.
- Supports command fields for presets, kernel values, activation rules, value override, and reset.

## Reconnection Model

ESP-WiFi-Mesh is explicitly configured as self-organized, with fixed-root mode disabled. That lets the mesh select a root, select parents, and initiate reconnection after parent loss. The firmware adds an application-level watchdog so temporary distance, interference, weak power, or router/MQTT outages are handled in stages:

- `MESH_HEALTH_CHECK_INTERVAL_MS`: health check period, default 10 seconds.
- `MESH_RECONNECT_ATTEMPT_MS`: detached duration before explicitly calling `esp_mesh_connect()`, default 15 seconds.
- `MESH_RECONNECT_RESTART_MS`: detached duration before restarting the mesh stack, default 60 seconds.
- `MESH_ROOT_IP_RECOVERY_MS`: root grace period before forcing router-side recovery, default 45 seconds.
- `MESH_AP_ASSOC_EXPIRE_SECONDS`: parent association timeout, default 30 seconds, so brief quiet periods do not immediately drop a child.

If `esp_mesh_get_layer()` stays at 0, the node first asks ESP-WiFi-Mesh to reconnect. If it remains detached after the restart grace period, the firmware stops and restarts the mesh stack, then resumes normal join attempts. Parent disconnect events clear stale cached root addresses so nodes do not keep sending to an old root.

On recovery events, the node publishes immediately instead of waiting for the next 5-second heartbeat. During a mesh partition, the root's `routing_table_size` may temporarily shrink; the visualization reflects the mesh seen by the active root and fills back in as nodes rejoin.

## Scaling Model

The code is designed so a 400-1000 node deployment does not require each ESP32 to store or publish the entire mesh.

- `MAX_NEIGHBORS` is the local neighborhood size used for activation/topology samples.
- `MESH_TARGET_NODE_COUNT` documents the intended scale target.
- `routing_table_size` in `mesh/topology` reports the full mesh count visible to the root.
- `routing_table` and `routing_table_sample` are bounded samples, capped by `MQTT_TOPOLOGY_ROUTE_SAMPLE_LIMIT`.
- `routing_table_truncated` tells consumers whether the sample is incomplete.

This means the visualization should use `routing_table_size` for full mesh count and the sampled arrays only for rendering hints.

## Visualization Server

The Node.js server receives:

- MQTT state and topology messages from the broker.
- Legacy binary UDP frames from root forwarding.

It stores the current node map in memory and exposes:

- `GET /api/nodes`
- `GET /api/topology`
- `GET /api/stats`
- `GET /api/mqtt`

It broadcasts browser updates over WebSocket:

- `full_state`
- `node_update`
- `stats`

Topology messages can reference nodes before their full `mesh/state` arrives. The server creates placeholder nodes for these references so newly joined devices appear in the visualization promptly. Stale nodes are removed after `STALE_NODE_TIMEOUT_MS`, default 30 seconds.

## Data Flow

### Periodic State

```text
Non-root node
  -> mesh_message_t via ESP-WiFi-Mesh
  -> root mesh_receive_task
  -> mqtt_publish_node_state()
  -> mesh/state
  -> visualization server
  -> WebSocket node_update
```

### Topology

```text
Root
  -> esp_mesh_get_routing_table_size()
  -> bounded esp_mesh_get_routing_table() sample
  -> mesh/topology
  -> visualization placeholder nodes and links
```

### Commands

```text
MQTT publisher
  -> mesh/commands or mesh/commands/<root-mac>
  -> root MQTT client
  -> local state/kernel/activation update
  -> propagated through mesh state sequence adoption
```

## Known Constraints

- The visualization server is in-memory. Restarting it clears node history.
- Only the current root connects to MQTT.
- A topology sample is not the full topology when `routing_table_truncated` is true.
- The legacy UDP binary protocol is smaller than the internal mesh message and does not include activation kernels.
- Very large deployments need broker/server/rendering rate control beyond the firmware-side bounded samples.
