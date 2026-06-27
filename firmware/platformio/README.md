# ESP32 Mesh Node Firmware - PlatformIO

PlatformIO project for the Adaptive ESP32 Mesh Network Node.

## Features

- ✅ **Multi-Platform Support**: ESP32-C3 (2.4 GHz), ESP32-C5 (5 GHz), Generic ESP32
- ✅ **PlatformIO Compatible**: Easy development with VS Code
- ✅ **ESP-WiFi-Mesh**: Decentralized mesh networking
- ✅ **Dynamic Neighbor Management**: Top 8 closest neighbors by RSSI
- ✅ **UDP Unicast**: Efficient state updates to neighbors only
- ✅ **Mesh Forwarding**: Non-neighbor packets forwarded through mesh
- ✅ **Dynamic TX Power**: Adjusts based on weakest neighbor RSSI
- ✅ **Sensor Support**: Temperature and mmWave sensors
- ✅ **RGB LED Control**: Visual state indication

## Platform Support

| Platform | WiFi Band | Channel Range | Status |
|----------|-----------|---------------|--------|
| ESP32-C3 | 2.4 GHz | 1-14 | ✅ Supported |
| ESP32-C5 | 5 GHz | 36-165 | ✅ Supported |
| ESP32 | 2.4 GHz | 1-14 | ✅ Supported |

## Quick Start

### 1. Install Prerequisites

- [VS Code](https://code.visualstudio.com/)
- [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)
- [ESP-IDF](https://docs.platformio.org/en/latest/frameworks/esp-idf.html) (automatically installed by PlatformIO)

### 2. Open Project in VS Code

1. Open VS Code
2. Install PlatformIO extension
3. Open this folder: `firmware/platformio`
4. PlatformIO will automatically install dependencies

### 3. Select Target Platform

Use the PlatformIO toolbar at the bottom of VS Code to select your target:

- **ESP32-C3**: `esp32c3_devkitm_1` (2.4 GHz)
- **ESP32-C5**: `esp32c5_devkitm_1` (5 GHz)
- **Generic ESP32**: `esp32dev` (2.4 GHz)

### 4. Configure WiFi Channel

Edit `platformio.ini` to set the appropriate channel:

```ini
; For ESP32-C3 (2.4 GHz)
build_flags =
    -DESP_PLATFORM=ESP32_C3
    -DWIFI_CHANNEL=6  ; 2.4 GHz channel 1-14

; For ESP32-C5 (5 GHz)
build_flags =
    -DESP_PLATFORM=ESP32_C5
    -DWIFI_CHANNEL=36  ; 5 GHz channel 36-165
```

### 5. Configure Visualization Server

Edit `platformio.ini` to set your visualization server IP:

```ini
build_flags =
    -DVISUALIZATION_IP="192.168.1.254"
```

### 6. Build and Flash

#### Using PlatformIO Toolbar:
1. Click the PlatformIO icon in the sidebar
2. Select your environment (e.g., `esp32c3_devkitm_1`)
3. Click **Build** (checkmark icon)
4. Click **Upload** (arrow icon)
5. Click **Monitor** (plug icon) to see serial output

#### Using Command Line:
```bash
# Build
pio run -e esp32c3_devkitm_1

# Flash
pio run -e esp32c3_devkitm_1 -t upload

# Monitor
pio device monitor -e esp32c3_devkitm_1
```

## Project Structure

```
firmware/platformio/
├── platformio.ini              # PlatformIO configuration
├── README.md                   # This file
├── src/
│   ├── main.cpp                # Main entry point
│   ├── mesh_node.cpp           # Core mesh logic
│   ├── beacon_monitor.cpp      # Beacon frame scanning
│   └── state_manager.cpp       # Node state and sensors
└── include/
    ├── config.h                # Configuration (platform-aware)
    ├── mesh_node.h
    ├── beacon_monitor.h
    └── state_manager.h
```

## Configuration Options

### WiFi and Mesh Settings

| Option | Default | Description |
|--------|---------|-------------|
| `WIFI_CHANNEL` | 6 (2.4 GHz) / 36 (5 GHz) | WiFi channel |
| `MESH_MAX_HOPS` | 10 | Maximum mesh hops |
| `MESH_VOTE_PERCENT` | 1 | Self-healing percentage |
| `MESH_ROUTER_SSID` | "mesh_network" | Mesh network SSID |
| `MESH_ROUTER_PASS` | "mesh_password" | Mesh network password |

### UDP Settings

| Option | Default | Description |
|--------|---------|-------------|
| `UDP_PORT` | 1234 | UDP port for state updates |
| `VISUALIZATION_IP` | "192.168.1.254" | Visualization server IP |
| `VISUALIZATION_PORT` | 1234 | Visualization server port |

### Neighbor Settings

| Option | Default | Description |
|--------|---------|-------------|
| `MAX_NEIGHBORS` | 8 | Maximum neighbors per node |
| `RSSI_THRESHOLD` | 3 | dBm change to trigger update |
| `NEIGHBOR_TIMEOUT_MS` | 5000 | Remove after 5s inactivity |

### TX Power Settings

| Option | Default | Description |
|--------|---------|-------------|
| `TARGET_RSSI` | -65 | Target RSSI at weakest neighbor |
| `MIN_TX_POWER` | 0 | Minimum TX power (dBm) |
| `MAX_TX_POWER` | 20 | Maximum TX power (dBm) |
| `TX_POWER_STEP` | 2 | TX power adjustment step (dBm) |

### State Update Settings

| Option | Default | Description |
|--------|---------|-------------|
| `STATE_UPDATE_INTERVAL` | 100 | ms between state updates (10 Hz) |
| `TEMPERATURE_SCALE` | 10.0 | Temperature scaling factor |

### Debug Settings

Enable debug logging by uncommenting in `platformio.ini`:

```ini
build_flags =
    -DDEBUG_MESH=true
    -DDEBUG_BEACON=true
    -DDEBUG_UDP=true
    -DDEBUG_NEIGHBORS=true
    -DDEBUG_TX_POWER=true
```

## Platform-Specific Notes

### ESP32-C3 (2.4 GHz)

- **Channel Range**: 1-14
- **Recommended Channel**: 6 (least interference)
- **Features**: WiFi, Bluetooth LE
- **Flash**: 4MB or 8MB
- **PSRAM**: Optional

**Pin Configuration:**
```cpp
// Default pins (can be changed in config.h)
#define LED_RED_PIN GPIO_NUM_1
#define LED_GREEN_PIN GPIO_NUM_2
#define LED_BLUE_PIN GPIO_NUM_3
#define TEMPERATURE_PIN GPIO_NUM_4
#define MMWAVE_PRESENCE_PIN GPIO_NUM_5
```

### ESP32-C5 (5 GHz)

- **Channel Range**: 36-165
- **Recommended Channel**: 36, 40, 44, 48 (DFS-free)
- **Features**: WiFi 6 (802.11ax), Bluetooth 5.2, 5 GHz support
- **Flash**: 4MB or 8MB
- **PSRAM**: Optional

**Pin Configuration:**
Same as ESP32-C3 by default, but can be customized.

### Generic ESP32 (2.4 GHz)

- **Channel Range**: 1-14
- **Recommended Channel**: 6, 11
- **Features**: WiFi, Bluetooth Classic + LE
- **Flash**: 4MB+
- **PSRAM**: Optional

## Debugging

### Serial Monitor

Use PlatformIO's built-in serial monitor:
1. Click the PlatformIO icon
2. Select your environment
3. Click **Monitor** (plug icon)
4. Set baud rate to 115200

### Common Debug Output

```
I (1234) MAIN: ========================================
I (1234) MAIN:   ESP32 Mesh Node - Adaptive Topology
I (1234) MAIN:   Platform: ESP32-C5
I (1234) MAIN:   WiFi Channel: 36
I (1234) MAIN:   Supports 2.4GHz: Yes
I (1234) MAIN:   Supports 5GHz: Yes
I (1234) MAIN: ========================================
I (2345) MESH_NODE: WiFi initialized, MAC: AA:BB:CC:DD:EE:FF
I (3456) MESH_NODE: ESP-WiFi-Mesh initialized
I (4567) MESH_NODE: UDP socket initialized on port 1234
I (5678) BEACON_MONITOR: Beacon monitoring initialized
I (6789) STATE_MANAGER: State manager initialized
```

### Enable Verbose Debugging

Add to `platformio.ini`:
```ini
build_flags =
    -DDEBUG_MESH=true
    -DDEBUG_BEACON=true
    -DDEBUG_UDP=true
    -DDEBUG_NEIGHBORS=true
    -DDEBUG_TX_POWER=true
```

## Troubleshooting

### Build Errors

1. **Missing ESP-IDF**: Run `pio platform install espressif32`
2. **Wrong Python version**: Use Python 3.7+
3. **Out of memory**: Reduce debug flags or increase stack size

### Flashing Errors

1. **Device not found**: Check USB connection and port
2. **Permission denied**: Add user to dialout group (Linux)
3. **Wrong board**: Select correct environment in PlatformIO

### Runtime Errors

1. **WiFi not connecting**: Check channel and credentials
2. **Mesh not forming**: Ensure all nodes have same configuration
3. **No UDP messages**: Check firewall and IP configuration

## Advanced Configuration

### Custom Partition Table

Create a `partitions.csv` file in the project root:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
phy_init, data, phy,     0xe000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
storage,  data, 0x99,    0x110000, 1M,
```

### Custom SDK Configuration

Create a `sdkconfig` file or use `menuconfig`:

```bash
pio run -e esp32c5_devkitm_1 -t menuconfig
```

### Over-the-Air (OTA) Updates

Add OTA support in `platformio.ini`:

```ini
[env:esp32c5_ota]
extends = env:esp32c5_devkitm_1
board_build.partitions = partitions_ota.csv
build_flags =
    -DOTA_ENABLED=1
```

## Testing

### Test with 2-3 Nodes

1. Flash all nodes with the same firmware
2. Power them on within range of each other
3. Check serial output for mesh formation
4. Verify nodes appear in visualization

### Test Scalability

1. Start with 2 nodes, verify connection
2. Add nodes one by one
3. Monitor mesh stability and performance
4. Test with 10+ nodes for larger networks

## Performance Tuning

### Reduce Memory Usage

```ini
build_flags =
    -Os  ; Optimize for size
    -DNDEBUG  ; Disable assertions
```

### Increase Stack Size

```ini
board_build.ldscript = custom.ld
```

Create `custom.ld` with increased stack sizes.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a Pull Request

## License

MIT License - see the main [LICENSE](../../LICENSE) file for details.

---

**Happy coding! 🚀**
