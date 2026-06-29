# Firmware

The firmware is a PlatformIO ESP-IDF project for Seeed XIAO ESP32-C3 mesh nodes.

## Layout

```text
firmware/platformio/
  platformio.ini
  partitions.csv
  include/
    config.h
    mesh_node.h
    mqtt_handler.h
    state_manager.h
    beacon_monitor.h
  src/
    main.cpp
    mesh_node.cpp
    mqtt_handler.cpp
    state_manager.cpp
    beacon_monitor.cpp
```

## Current Runtime Behavior

- Nodes use ESP-WiFi-Mesh tree topology.
- ESP-WiFi-Mesh elects a root dynamically.
- Non-root nodes send state upward to the current root.
- The root publishes all node states to MQTT on `mesh/state`.
- The root publishes bounded topology summaries on `mesh/topology`.
- Nodes update state every `STATE_UPDATE_INTERVAL`, publish/send every `MQTT_UPDATE_INTERVAL_MS`, and publish/send immediately on meaningful state changes.
- LED fade color is included in state JSON but does not trigger extra publishes by itself.
- Nodes actively reconnect using explicit ESP-WiFi-Mesh self-organization plus a firmware health watchdog. They should recover automatically after temporary range, interference, power, router, or MQTT outages.
- Firmware recovers common NVS issues and restarts if Wi-Fi returns a zero MAC.

## Build

```bash
cd firmware/platformio
PLATFORMIO_CORE_DIR=/tmp/pio-core PLATFORMIO_BUILD_DIR=/tmp/pio-build-mesh ~/.platformio/penv/bin/pio run
```

## Upload

Upload one node:

```bash
PLATFORMIO_CORE_DIR=/tmp/pio-core PLATFORMIO_BUILD_DIR=/tmp/pio-build-mesh ~/.platformio/penv/bin/pio run --target upload --upload-port /dev/ttyACM0
```

Upload all attached USB serial/JTAG boards:

```bash
./upload.sh
```

Monitor one node:

```bash
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200
```

## Configuration

Primary configuration lives in `platformio/include/config.h`.

Important settings:

| Setting | Meaning |
| --- | --- |
| `NETWORK_ENV_HOME` | Enables the home network defaults from `config.h` |
| `MESH_ROUTER_SSID`, `MESH_ROUTER_PASS` | Router credentials used by ESP-WiFi-Mesh |
| `MQTT_BROKER_IP`, `MQTT_BROKER_PORT` | Broker used by the elected root |
| `MESH_MAX_HOPS` | Maximum mesh depth |
| `MESH_AP_CONNECTIONS` | Maximum mesh children per node AP |
| `MAX_NEIGHBORS` | Bounded local neighbor table |
| `MQTT_UPDATE_INTERVAL_MS` | State heartbeat interval |
| `MESH_HEALTH_CHECK_INTERVAL_MS` | Mesh watchdog interval |
| `MESH_RECONNECT_ATTEMPT_MS` | Detached duration before explicit reconnect request |
| `MESH_RECONNECT_RESTART_MS` | Detached duration before mesh restart |
| `MESH_ROOT_IP_RECOVERY_MS` | Root IP recovery grace period |
| `MESH_AP_ASSOC_EXPIRE_SECONDS` | Quiet child association timeout |
| `MQTT_TOPOLOGY_ROUTE_SAMPLE_LIMIT` | Bounded routing sample size |

`MAX_NEIGHBORS` is not the total mesh size. It is a local memory/sample bound used for activation inputs and compact topology output.

## State and Activation

The node state includes:

- state code
- RGB color
- temperature x10
- mmWave presence/distance
- binary `value`
- activation sum
- kernel values
- activation rules
- sequence numbers for kernel/value/activation propagation

The activation/kernel behavior is inspired by `n2048-creative-technology/emergent-esp32`.

## Troubleshooting

### Node appears on USB but not in MQTT

1. Identify the MAC:

   ```bash
   for p in /dev/ttyACM* /dev/ttyUSB*; do
     ~/.platformio/packages/tool-esptoolpy/esptool.py --chip auto --port "$p" chip_id || true
   done
   ```

2. Monitor boot:

   ```bash
   ~/.platformio/penv/bin/pio device monitor -p <port> -b 115200
   ```

3. If logs show a zero MAC or NVS/Wi-Fi errors, erase flash once and upload again:

   ```bash
   ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32c3 --port <port> erase_flash
   PLATFORMIO_CORE_DIR=/tmp/pio-core PLATFORMIO_BUILD_DIR=/tmp/pio-build-mesh ~/.platformio/penv/bin/pio run --target upload --upload-port <port>
   ```

### Mesh shows fewer nodes after reboot/upload

ESP-WiFi-Mesh needs time to elect a root and reparent children. Nodes publish immediately when they attach/reparent and then resume the 5-second heartbeat. Check MQTT:

```bash
mosquitto_sub -h 192.168.178.169 -t 'mesh/#' -v
```

Look at `mesh/topology.routing_table_size` for the root's current mesh count.
