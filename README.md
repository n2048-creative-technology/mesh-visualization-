# Adaptive ESP32 Mesh Network with Real-Time Topology Visualization

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v4.4+-blue.svg)](https://github.com/espressif/esp-idf)
[![Node.js](https://img.shields.io/badge/Node.js-v16+-green.svg)](https://nodejs.org/)

**A fully decentralized, adaptive mesh network of 400+ ESP32 nodes with real-time web-based visualization.**

## Features

### Mesh Network
- ✅ **Decentralized Topology**: No root node, flat mesh architecture
- ✅ **Dynamic Neighbor Management**: Top 8 closest neighbors based on RSSI
- ✅ **UDP Unicast**: Efficient state updates only to closest neighbors
- ✅ **Mesh Forwarding**: Non-neighbor packets forwarded through the mesh
- ✅ **Dynamic TX Power**: Adjusts based on weakest neighbor RSSI
- ✅ **Self-Healing**: Automatic reconnection when nodes fail or move
- ✅ **5 GHz Support**: Optimized for ESP32-C5 on 5 GHz channels

### Visualization
- ✅ **Real-Time Updates**: 10 Hz updates via WebSocket
- ✅ **Interactive**: Click nodes for detailed information
- ✅ **Multiple Views**: Color by RSSI, state, temperature, or fixed
- ✅ **Force-Directed Layout**: Nodes repel, connections pull together
- ✅ **Responsive Design**: Works on desktop and mobile
- ✅ **Filtering**: Filter connections by minimum RSSI

### Sensors
- ✅ **Temperature**: Built-in or external temperature sensor
- ✅ **mmWave Radar**: Presence detection and distance measurement
- ✅ **RGB LED**: Visual state indication

## Repository Structure

```
emergent-esp32-mesh/
├── firmware/                    # ESP32 Node Firmware
│   ├── main/
│   │   ├── mesh_node.cpp       # Core mesh logic
│   │   ├── mesh_node.h
│   │   ├── beacon_monitor.cpp   # Beacon frame scanning
│   │   ├── beacon_monitor.h
│   │   ├── state_manager.cpp    # Node state and sensors
│   │   ├── state_manager.h
│   │   ├── config.h             # Configuration
│   │   └── main.cpp             # Entry point
│   ├── CMakeLists.txt
│   └── sdkconfig.defaults
├── visualization/               # Web Visualization
│   ├── app/
│   │   ├── public/
│   │   │   ├── index.html       # D3.js visualization
│   │   │   ├── style.css        # Styling
│   │   │   └── script.js        # Visualization logic
│   │   ├── server.js            # Node.js backend
│   │   └── package.json
│   └── README.md
├── docs/
│   ├── architecture.md          # System architecture
│   └── api.md                   # Message formats and APIs
└── README.md                    # This file
```

## Quick Start

### Prerequisites

#### For ESP32 Firmware
- [ESP-IDF v4.4+](https://github.com/espressif/esp-idf)
- [ESP32 toolchain](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
- ESP32 development board (ESP32-C5 recommended for 5 GHz)

#### For Visualization
- [Node.js v16+](https://nodejs.org/)
- npm or yarn

### Setup

#### 1. Clone the Repository

```bash
git clone https://github.com/n2048-creative-technology/mesh-visualization-.git
cd mesh-visualization-
```

#### 2. Set Up Visualization Server

```bash
cd visualization/app
npm install
npm start
```

The visualization server will start on `http://localhost:3000`.

#### 3. Build and Flash ESP32 Firmware

```bash
cd firmware
idf.py set-target esp32c5  # or esp32 for 2.4 GHz
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

#### 4. Configure Nodes

Edit `firmware/main/config.h` to configure:
- WiFi channel (36 for 5 GHz)
- Mesh settings
- UDP port
- Visualization server IP

#### 5. Test with Multiple Nodes

1. Flash 2-3 nodes with the firmware
2. Power them on
3. Open `http://localhost:3000` in your browser
4. Watch the mesh topology appear in real-time!

## Configuration

### ESP32 Node Configuration

Edit `firmware/main/config.h`:

```c
// WiFi and Mesh
#define WIFI_CHANNEL          36      // 5 GHz channel
#define MESH_MAX_HOPS          10
#define MESH_VOTE_PERCENT      1       // Self-healing

// UDP
#define UDP_PORT              1234
#define VISUALIZATION_IP      "192.168.1.254"  // Your laptop/RPi IP

// Neighbor Settings
#define MAX_NEIGHBORS          8
#define RSSI_THRESHOLD         3       // dBm change to trigger update
#define NEIGHBOR_TIMEOUT_MS    5000    // Remove after 5s inactivity

// TX Power
#define TARGET_RSSI           -65      // Target RSSI at weakest neighbor
#define MIN_TX_POWER          0       // dBm
#define MAX_TX_POWER          20      // dBm

// State Updates
#define STATE_UPDATE_INTERVAL 100     // ms (10 Hz)
```

### Visualization Server Configuration

Edit `visualization/app/server.js`:

```javascript
const UDP_PORT = 1234;
const HTTP_PORT = 3000;
const WS_PORT = 3000;
```

## Usage

### Starting the Visualization

```bash
cd visualization/app
npm start
```

Then open `http://localhost:3000` in your browser.

### Flashing a Node

```bash
cd firmware
idf.py -p /dev/ttyUSB0 flash monitor
```

### Development Mode

For automatic restart on code changes:

```bash
cd visualization/app
npm run dev
```

## Message Protocol

### Binary UDP Message (77 bytes)

```c
struct NodeUpdate {
    uint8_t version;         // Protocol version (0x01)
    uint8_t msg_type;        // Message type (0x01 = state update)
    uint8_t mac[6];          // Node MAC address
    uint8_t state;           // Node state (0=idle, 1=active, 2=error, 3=booting)
    uint8_t color[3];        // RGB color
    int16_t temperature;     // Temperature × 10
    uint8_t mmwave_presence; // Presence detection
    uint32_t mmwave_distance; // Distance in mm
    uint32_t timestamp;      // Unix timestamp
    struct {
        uint8_t mac[6];      // Neighbor MAC
        int8_t rssi;         // RSSI in dBm
    } neighbors[8];          // Top 8 neighbors
    uint16_t checksum;       // Checksum
};
```

### WebSocket Messages

- `full_state`: Initial state sent to new clients
- `node_update`: Single node update
- `stats`: Periodic statistics

## API Reference

See [docs/api.md](docs/api.md) for complete API documentation.

## Architecture

See [docs/architecture.md](docs/architecture.md) for system architecture details.

## Troubleshooting

### Common Issues

#### Nodes not appearing in visualization

1. Check that the visualization server is running
2. Verify the UDP port (1234) is not blocked by firewall
3. Check that `VISUALIZATION_IP` in `config.h` matches your server IP
4. Use Wireshark to verify UDP messages are being sent

#### Mesh not forming

1. Ensure all nodes are on the same WiFi channel
2. Check that nodes are within range of each other
3. Verify mesh configuration in `config.h`
4. Check ESP-IDF version (v4.4+ required)

#### Compilation errors

1. Ensure you have the correct ESP-IDF version
2. Check that all required components are enabled
3. Verify toolchain is properly installed

### Debugging

Enable debug logging in `config.h`:

```c
#define DEBUG_MESH            true
#define DEBUG_BEACON          true
#define DEBUG_UDP             true
#define DEBUG_NEIGHBORS       true
#define DEBUG_TX_POWER        true
```

View debug output with:

```bash
idf.py monitor
```

## Performance

### Message Rates
- **State Updates**: 10 Hz per node
- **Message Size**: 77 bytes
- **400 Nodes**: ~308 KB/second total

### Optimization
- Binary protocol minimizes message size
- UDP unicast to only 8 closest neighbors
- Client-side filtering reduces rendering load
- Force simulation optimized for real-time updates

## Scalability

| Nodes | Tested | Expected Performance |
|-------|--------|---------------------|
| 2-10 | ✅ Yes | Excellent |
| 10-50 | ✅ Yes | Very Good |
| 50-100 | ⚠️ Partial | Good |
| 100-400 | 🎯 Target | Moderate |
| 400+ | ❓ Untested | May need optimization |

## Security

### Current Implementation
- No encryption (plaintext messages)
- No authentication (any node can send messages)
- Simple checksum for integrity

### Recommended Enhancements
- AES-128 message encryption
- Node authentication (shared secret or certificates)
- HMAC message signing
- Network isolation (separate VLAN)
- Firewall rules

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [Espressif Systems](https://www.espressif.com/) for ESP32 and ESP-IDF
- [D3.js](https://d3js.org/) for visualization
- [Node.js](https://nodejs.org/) for backend
- All contributors and testers

## Contact

For questions or support, please open an issue on GitHub.

---

**Built with ❤️ for adaptive mesh networks**
