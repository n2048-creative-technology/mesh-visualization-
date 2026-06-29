# PlatformIO Firmware Project

This folder contains the ESP-IDF firmware built with PlatformIO.

## Target

Current active target:

- Board: `seeed_xiao_esp32c3`
- MCU: ESP32-C3
- Framework: ESP-IDF
- Upload protocol: `esptool`
- Monitor speed: `115200`

The default environment is defined in `platformio.ini`:

```ini
[platformio]
default_envs = seeed_xiao_esp32c3
```

## Build

```bash
PLATFORMIO_CORE_DIR=/tmp/pio-core PLATFORMIO_BUILD_DIR=/tmp/pio-build-mesh ~/.platformio/penv/bin/pio run
```

The custom `PLATFORMIO_CORE_DIR` and `PLATFORMIO_BUILD_DIR` are optional, but useful for keeping generated build output outside the repository.

## Upload

One node:

```bash
PLATFORMIO_CORE_DIR=/tmp/pio-core PLATFORMIO_BUILD_DIR=/tmp/pio-build-mesh ~/.platformio/penv/bin/pio run --target upload --upload-port /dev/ttyACM0
```

All attached USB serial/JTAG nodes:

```bash
./upload.sh
```

Identify attached boards:

```bash
for p in /dev/ttyACM*; do
  echo "=== $p ==="
  ~/.platformio/packages/tool-esptoolpy/esptool.py --chip auto --port "$p" chip_id || true
done
```

## Monitor

```bash
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200
```

If PlatformIO monitor is run from automation, it may need a pseudo-terminal.

## Current Firmware Behavior

- ESP-WiFi-Mesh tree topology with dynamic root election.
- Root connects to router, starts MQTT, and publishes state/topology.
- Non-root nodes send `mesh_message_t` upward to the root.
- State is sent every `MQTT_UPDATE_INTERVAL_MS` and on meaningful state changes.
- Mesh attach, root address, routing-table, child attach/detach, and root IP events request an immediate state/topology publish.
- LED fade color does not trigger early state publishes.
- ESP-WiFi-Mesh self-organization is explicitly enabled and fixed-root mode is disabled.
- Mesh health watchdog requests reconnect after short detachment and restarts the mesh stack after long detachment.
- NVS initialization recovers common NVS errors; zero Wi-Fi MAC triggers NVS erase and restart.

## Important Configuration

Configuration is in `include/config.h`.

| Option | Default/current role |
| --- | --- |
| `PLATFORM_NAME` | `Seeed XIAO ESP32-C3` |
| `WIFI_CHANNEL` | `0` in home mode, allowing router scan |
| `MESH_ROUTER_SSID` | Router SSID |
| `MESH_ROUTER_PASS` | Router password |
| `MESH_MAX_HOPS` | Maximum mesh layers |
| `MESH_AP_CONNECTIONS` | Max mesh children per AP |
| `MAX_NEIGHBORS` | Local bounded neighbor table |
| `MQTT_BROKER_IP` | Broker used by root |
| `MQTT_UPDATE_INTERVAL_MS` | 5-second heartbeat |
| `MESH_HEALTH_CHECK_INTERVAL_MS` | 10-second health check |
| `MESH_RECONNECT_ATTEMPT_MS` | 15-second detached reconnect request |
| `MESH_RECONNECT_RESTART_MS` | 60-second detached restart |
| `MESH_ROOT_IP_RECOVERY_MS` | 45-second root IP recovery grace period |
| `MESH_AP_ASSOC_EXPIRE_SECONDS` | 30-second quiet child association timeout |
| `MQTT_TOPOLOGY_ROUTE_SAMPLE_LIMIT` | Bounded route sample size |
| `MMWAVE_PRESENCE_PIN` | GPIO 5 |
| `DATA_PIN` | WS2812 GPIO 10 |

## MQTT Output

`mesh/state` contains full node state, activation kernel, activation rules, value, activation sum, and RGB color.

`mesh/topology` contains the root state, layer, full `routing_table_size`, bounded route samples, bounded neighbor samples, and a truncation flag.

## Flash Warning

PlatformIO may print:

```text
Flash memory size mismatch detected. Expected 4MB, found 2MB
```

The current firmware has still been building and booting in the active layout. Treat this as a configuration warning to revisit before production-scale deployment, not as proof that upload failed.

## Recovery

If a node boots with Wi-Fi/NVS errors or logs MAC `00:00:00:00:00:00`, erase flash once:

```bash
~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32c3 --port <port> erase_flash
```

Then upload again.
