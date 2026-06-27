# ESP32 Mesh Node Firmware

This directory contains the firmware for ESP32 mesh nodes.

## PlatformIO Project

The firmware is organized as a **PlatformIO project** for easy development with **Visual Studio Code**.

### Project Structure

```
firmware/
├── README.md                   # This file
└── platformio/
    ├── platformio.ini          # PlatformIO configuration
    ├── CMakeLists.txt           # CMake configuration (for IDE)
    ├── README.md               # PlatformIO-specific documentation
    ├── .vscode/
    │   └── settings.json        # VS Code settings
    ├── include/
    │   ├── config.h            # Platform-aware configuration
    │   ├── mesh_node.h
    │   ├── beacon_monitor.h
    │   └── state_manager.h
    └── src/
        ├── main.cpp
        ├── mesh_node.cpp
        ├── beacon_monitor.cpp
        └── state_manager.cpp
```

## Quick Start

### 1. Install Prerequisites

- [VS Code](https://code.visualstudio.com/)
- [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)

### 2. Open Project

1. Open VS Code
2. Install PlatformIO extension
3. Open the `firmware/platformio` folder
4. PlatformIO will automatically install dependencies

### 3. Select Target Platform

Use the PlatformIO toolbar to select your target:

- **ESP32-C3**: `esp32c3_devkitm_1` (2.4 GHz)
- **ESP32-C5**: `esp32c5_devkitm_1` (5 GHz)
- **Generic ESP32**: `esp32dev` (2.4 GHz)

### 4. Build and Flash

#### Using PlatformIO Toolbar:
1. Click the PlatformIO icon in the sidebar
2. Select your environment
3. Click **Build** (✓ icon)
4. Click **Upload** (→ icon)
5. Click **Monitor** (🔌 icon) to see serial output

#### Using Command Line:
```bash
# Navigate to platformio directory
cd firmware/platformio

# Build for ESP32-C3 (2.4 GHz)
pio run -e esp32c3_devkitm_1

# Build for ESP32-C5 (5 GHz)
pio run -e esp32c5_devkitm_1

# Flash
pio run -e esp32c3_devkitm_1 -t upload

# Monitor
pio device monitor -e esp32c3_devkitm_1
```

## Platform Support

| Platform | WiFi Band | Channel Range | Environment | Status |
|----------|-----------|---------------|------------|--------|
| ESP32-C3 | 2.4 GHz | 1-14 | `esp32c3_devkitm_1` | ✅ Supported |
| ESP32-C5 | 5 GHz | 36-165 | `esp32c5_devkitm_1` | ✅ Supported |
| ESP32 | 2.4 GHz | 1-14 | `esp32dev` | ✅ Supported |

## Configuration

### WiFi Channel Configuration

The WiFi channel is automatically set based on the platform:

- **ESP32-C3**: Channel 6 (2.4 GHz)
- **ESP32-C5**: Channel 36 (5 GHz)
- **Generic ESP32**: Channel 6 (2.4 GHz)

You can override this in `platformio.ini`:

```ini
[env:esp32c3_devkitm_1]
build_flags =
    -DWIFI_CHANNEL=11  ; Use channel 11 instead of 6
```

### Visualization Server IP

Set the visualization server IP in `platformio.ini`:

```ini
[env:esp32c3_devkitm_1]
build_flags =
    -DVISUALIZATION_IP="192.168.1.100"
```

### Debug Logging

Enable debug logging in `platformio.ini`:

```ini
[env:esp32c3_devkitm_1]
build_flags =
    -DDEBUG_MESH=true
    -DDEBUG_BEACON=true
    -DDEBUG_UDP=true
    -DDEBUG_NEIGHBORS=true
    -DDEBUG_TX_POWER=true
```

## Features

### Core Functionality
- ✅ **ESP-WiFi-Mesh**: Decentralized mesh networking
- ✅ **Dynamic Neighbor Management**: Top 8 closest neighbors by RSSI
- ✅ **UDP Unicast**: Efficient state updates to neighbors only
- ✅ **Mesh Forwarding**: Non-neighbor packets forwarded through mesh
- ✅ **Dynamic TX Power**: Adjusts based on weakest neighbor RSSI
- ✅ **Self-Healing**: Automatic reconnection when nodes fail

### Sensor Support
- ✅ **Temperature Sensor**: ADC-based temperature reading
- ✅ **mmWave Radar**: Presence detection and distance measurement
- ✅ **RGB LED**: Visual state indication

### Protocol
- ✅ **Binary Protocol**: Efficient 77-byte messages
- ✅ **Checksum**: Message integrity verification
- ✅ **Platform-Aware**: Automatic configuration for ESP32-C3/C5

## Pin Configuration

Default pin assignments (can be changed in `include/config.h`):

| Function | Pin | Description |
|----------|-----|-------------|
| LED_RED | GPIO 1 | Red LED |
| LED_GREEN | GPIO 2 | Green LED |
| LED_BLUE | GPIO 3 | Blue LED |
| TEMPERATURE | GPIO 4 | Temperature sensor |
| MMWAVE_PRESENCE | GPIO 5 | mmWave presence detection |

## Documentation

- [PlatformIO Setup](platformio/README.md) - Detailed PlatformIO configuration
- [API Documentation](../../docs/api.md) - Message formats and protocols
- [Architecture](../../docs/architecture.md) - System architecture

## Troubleshooting

### Build Errors

1. **Missing PlatformIO**: Install PlatformIO extension in VS Code
2. **Missing ESP-IDF**: PlatformIO will install it automatically
3. **Python version**: Use Python 3.7+

### Flashing Errors

1. **Device not found**: Check USB connection
2. **Permission denied**: Add user to dialout group (Linux)
3. **Wrong board**: Select correct environment in PlatformIO

### Runtime Errors

1. **WiFi not connecting**: Check channel and credentials
2. **Mesh not forming**: Ensure all nodes have same configuration
3. **No UDP messages**: Check firewall and IP configuration

## VS Code Tips

### Recommended Extensions
- PlatformIO
- C/C++ (Microsoft)
- CMake Tools
- Clang-Format
- GitLens

### Keybindings
- `Ctrl+Shift+P`: Command palette
- `Ctrl+Shift+B`: Build
- `Ctrl+Alt+U`: Upload
- `Ctrl+Shift+M`: Monitor

## License

MIT License - see the main [LICENSE](../../LICENSE) file for details.

---

**For detailed PlatformIO configuration, see [platformio/README.md](platformio/README.md)**
