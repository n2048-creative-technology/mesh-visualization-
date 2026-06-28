# ESP32C3 Mesh Network with Real-Time Visualization and MQTT Control

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v4.4+-blue.svg)](https://github.com/espressif/esp-idf)
[![ESP32C3](https://img.shields.io/badge/ESP32C3-Mesh-blue.svg)](https://www.espressif.com/en/products/socs/esp32-c3)
[![MQTT](https://img.shields.io/badge/MQTT-Mosquitto-green.svg)](https://mosquitto.org/)
[![Node.js](https://img.shields.io/badge/Node.js-v16+-green.svg)](https://nodejs.org/)

**A large-scale, adaptive mesh network system using 400-1000 ESP32C3 nodes with real-time topology visualization and MQTT-based administrative control.**

## System Overview

This project implements a **fully decentralized mesh network** using **Espressif's ESP-WiFi-Mesh** protocol on **ESP32C3** microcontrollers. The system is designed to operate with **400 to 1000 nodes**, forming a self-organizing, self-healing mesh that maintains connectivity even when individual nodes fail or move.

### Core Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            MESH NETWORK (400-1000 nodes)                  │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                  │
│  │ ESP32C3     │    │ ESP32C3     │    │ ESP32C3     │                  │
│  │ Node A      │◄──►│ Node B      │◄──►│ Node C      │                  │
│  │ - Status    │    │ - Status    │    │ - Status    │                  │
│  │ - Temp      │    │ - Temp      │    │ - Temp      │                  │
│  │ - mmWave    │    │ - mmWave    │    │ - mmWave    │                  │
│  │ - 8 Neighbors│   │ - 8 Neighbors│   │ - 8 Neighbors│                  │
│  └─────────────┘    └─────────────┘    └─────────────┘                  │
└─────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        WiFi Router (When Available)                       │
│                     ┌─────────────────────┐                               │
│                     │  SSID & Password     │◄── Build-time Configuration   │
│                     │  (Set as build flags)│                               │
│                     └─────────────────────┘                               │
└─────────────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│  MQTT Broker     │ │ Node.js          │ │ Admin Panel      │
│  (Mosquitto)     │ │ Visualization    │ │ (Browser-based)  │
│                 │ │ Application      │ │                 │
│ - Receives node │ │ - Real-time mesh │ │ - Publishes rule │
│   status updates │ │   topology       │ │   updates via    │
│ - Forwards admin│ │ - D3.js based    │ │   MQTT           │
│   commands       │ │ - WebSocket      │ │ - Controls node  │
│                 │ │   updates        │ │   behavior       │
└─────────────────┘ └─────────────────┘ └─────────────────┘
```

## Key Features

### 🔗 Mesh Network Layer
- **ESP-WiFi-Mesh Protocol**: Espressif's proprietary mesh networking stack
- **400-1000 Nodes**: Scalable architecture for large deployments
- **ESP32C3 Hardware**: Low-cost, WiFi 4 capable microcontroller
- **Dynamic Topology**: Nodes automatically discover and maintain connections
- **Self-Healing**: Network reconfigures around failed or moved nodes

### 📡 Neighbor Management
- **RSSI-Based Selection**: Each node maintains a list of its **8 closest neighbors**
- **Continuous Scanning**: Nodes monitor beacon frames from all nearby nodes
- **Real-Time Updates**: Neighbor list updates based on signal strength (RSSI)
- **Decentralized**: No central coordinator - each node makes independent decisions

### 📊 Node Status System
Each node maintains a **status struct** in memory containing:

```c
struct NodeStatus {
    bool active;           // Current status (true/false)
    float temperature;     // Processor temperature (°C)
    bool mmwave_proximity; // Human proximity detected (true/false)
};
```

**Status Update Logic:**
- Status is updated via **OR operation**: `status = status OR mmwave_proximity`
- When mmWave sensor detects human proximity → status is set to `true`
- Status changes are **instantly shared** with all 8 closest neighbors
- Each neighbor, upon receiving a status update, triggers its own status update function

### 🔄 Communication Flow

#### Always-On (Even Without Router/MQTT)
1. **Beacon Monitoring**: Nodes continuously scan for beacon frames from other nodes
2. **Neighbor Discovery**: Each node maintains updated list of 8 closest neighbors by RSSI
3. **Status Sharing**: Any status change is immediately broadcast to all 8 neighbors
4. **Status Propagation**: Neighbors update their internal status upon receiving updates

#### When Connected to Router
1. **WiFi Connection**: Nodes connect to configured WiFi network (SSID/password via build flags)
2. **MQTT Connection**: Nodes connect to MQTT broker (Mosquitto)
3. **Status Publishing**: Nodes publish their status to MQTT topics
4. **Admin Control**: Nodes subscribe to MQTT topics for administrative commands

### 🎛️ Administrative Control
- **Browser-Based Admin Panel**: Web interface for system configuration
- **MQTT Message Format**: Admin commands sent as MQTT messages to nodes
- **Rule Updates**: Administrative updates modify the **rules and activation functions** used during status update calls
- **Real-Time Configuration**: Changes take effect immediately across the mesh

### 🖥️ Real-Time Visualization
- **Node.js Backend**: Handles MQTT subscriptions and WebSocket connections
- **D3.js Frontend**: Interactive, real-time visualization of mesh topology
- **Web-Based**: Runs in any modern browser
- **Live Updates**: Shows node status, temperature, proximity, and connections

## Repository Structure

```
mesh-visualization/
├── firmware/                          # ESP32C3 Node Firmware
│   ├── main/
│   │   ├── mesh_node.cpp              # ESP-WiFi-Mesh core logic
│   │   ├── mesh_node.h
│   │   ├── neighbor_manager.cpp        # 8-closest neighbors by RSSI
│   │   ├── neighbor_manager.h
│   │   ├── status_manager.cpp          # Status struct & update logic
│   │   ├── status_manager.h
│   │   ├── mmwave_sensor.cpp           # mmWave proximity detection
│   │   ├── mmwave_sensor.h
│   │   ├── temperature_sensor.cpp      # Processor temperature
│   │   ├── temperature_sensor.h
│   │   ├── mqtt_client.cpp             # MQTT connection & messaging
│   │   ├── mqtt_client.h
│   │   ├── config.h                    # Build-time configuration
│   │   └── main.cpp                    # Entry point
│   ├── CMakeLists.txt
│   └── sdkconfig.defaults
│
├── visualization/                     # Web Visualization & Admin
│   ├── app/
│   │   ├── public/
│   │   │   ├── index.html              # D3.js visualization + Admin panel
│   │   │   ├── style.css               # Styling
│   │   │   └── script.js               # Visualization & MQTT logic
│   │   ├── server.js                   # Node.js backend (MQTT + WebSocket)
│   │   └── package.json
│   └── README.md
│
├── mqtt-broker/                       # Mosquitto MQTT Broker
│   ├── mosquitto.conf                 # Broker configuration
│   └── README.md
│
├── docs/
│   ├── architecture.md                 # Detailed system architecture
│   ├── message-protocol.md             # MQTT & mesh message formats
│   ├── api.md                          # Admin API documentation
│   └── deployment.md                   # Large-scale deployment guide
│
└── README.md                           # This file
```

## Hardware Requirements

### Per Node
- **ESP32C3** microcontroller (or compatible ESP32 variant)
- **mmWave Radar Sensor** (e.g., LD2410, HLK-LD2410, or similar 24GHz sensor)
- **Power Supply**: 5V USB or battery (depending on deployment)
- **Optional**: RGB LED for visual status indication

### Central System
- **Computer** (Linux/Windows/macOS) running:
  - Mosquitto MQTT Broker
  - Node.js visualization application
- **WiFi Router** (2.4GHz or 5GHz, depending on configuration)
- **Browser** for admin panel and visualization

## Software Requirements

### For ESP32C3 Firmware
- [ESP-IDF v4.4+](https://github.com/espressif/esp-idf)
- [ESP32 toolchain](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
- PlatformIO (for building with `pio run`)

### For Central System
- [Node.js v16+](https://nodejs.org/)
- [Mosquitto MQTT Broker](https://mosquitto.org/)
- npm or yarn

## Quick Start

### 1. Clone the Repository

```bash
git clone https://github.com/n2048-creative-technology/mesh-visualization-.git
cd mesh-visualization-
```

### 2. Set Up MQTT Broker

Install Mosquitto:
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install mosquitto mosquitto-clients

# macOS
brew install mosquitto
```

Start the broker:
```bash
mosquitto -c mqtt-broker/mosquitto.conf -d
```

### 3. Set Up Visualization Server

```bash
cd visualization/app
npm install
npm start
```

The server will start on `http://localhost:3000` with both visualization and admin panel.

### 4. Configure and Build Firmware

Edit `firmware/main/config.h` to set your WiFi credentials and MQTT settings:

```c
// WiFi Configuration (set as build flags or in config.h)
#define WIFI_SSID "your_ssid"
#define WIFI_PASSWORD "your_password"

// MQTT Configuration
#define MQTT_BROKER_IP "192.168.1.100"  // Your computer's IP
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "esp32c3_node_%06X"  // Unique per node

// Mesh Configuration
#define MESH_MAX_HOPS 10
#define MAX_NEIGHBORS 8
#define BEACON_INTERVAL_MS 1000

// Sensor Configuration
#define TEMPERATURE_UPDATE_INTERVAL_MS 5000
#define MMWAVE_UPDATE_INTERVAL_MS 1000
```

Build the firmware with PlatformIO:
```bash
cd firmware
pio run
```

### 5. Flash and Deploy Nodes

Flash a node:
```bash
pio run --target upload -e esp32c3dev
```

Or using idf.py:
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Deploy 400-1000 nodes in your environment. They will automatically form the mesh network.

### 6. Access the System

- **Visualization**: `http://localhost:3000` - Real-time mesh topology
- **Admin Panel**: `http://localhost:3000/admin` - Configure rules and behavior
- **MQTT Topics**: Monitor with `mosquitto_sub -t "mesh/#" -v`

## Configuration Details

### Build-Time Flags (PlatformIO)

Set these in `platformio.ini` or as command-line arguments:

```ini
[env]
build_flags =
    -DWIFI_SSID="your_ssid"
    -DWIFI_PASSWORD="your_password"
    -DMQTT_BROKER_IP="192.168.1.100"
    -DMESH_CHANNEL=6
    -DMAX_NEIGHBORS=8
```

Build with custom flags:
```bash
pio run -DWIFI_SSID=my_ssid -DWIFI_PASSWORD=my_password
```

### Node Configuration Structure

```c
// In firmware/main/config.h
typedef struct {
    const char* wifi_ssid;
    const char* wifi_password;
    const char* mqtt_broker_ip;
    int mqtt_port;
    int mesh_channel;
    int max_neighbors;
    int beacon_interval_ms;
    int status_update_interval_ms;
} NodeConfig;
```

## Message Protocol

### Mesh Communication (Always Active)

Nodes communicate directly via ESP-WiFi-Mesh protocol. No central coordination required.

**Beacon Frame Structure:**
```c
struct MeshBeacon {
    uint8_t mac[6];      // Source node MAC address
    int8_t rssi;         // Received signal strength
    uint32_t timestamp;  // Beacon timestamp
};
```

**Status Update Message (Sent to 8 neighbors):**
```c
struct StatusUpdate {
    uint8_t mac[6];              // Sender MAC
    bool status;                // Current status (true/false)
    float temperature;          // Processor temperature
    bool mmwave_proximity;      // Human proximity detected
    uint32_t timestamp;         // Update timestamp
    uint8_t neighbor_count;      // Number of neighbors (max 8)
    struct {
        uint8_t mac[6];
        int8_t rssi;
    } neighbors[8];             // Top 8 neighbors by RSSI
};
```

### MQTT Communication (When Connected to Router)

**Topics:**
```
mesh/status/{node_mac}      # Node publishes its status
mesh/rules/update          # Admin publishes rule updates
mesh/command/{node_mac}    # Direct commands to specific node
mesh/broadcast             # Broadcast to all nodes
```

**Status Message (Node → MQTT):**
```json
{
    "mac": "A4:CF:12:34:56:78",
    "status": true,
    "temperature": 45.2,
    "mmwave_proximity": true,
    "neighbors": [
        {"mac": "B0:D5:CC:22:11:00", "rssi": -55},
        {"mac": "A8:33:22:11:44:55", "rssi": -62}
    ],
    "timestamp": 1699900000
}
```

**Rule Update Message (Admin → MQTT):**
```json
{
    "rule_id": "proximity_activation",
    "activation_function": "OR",
    "parameters": {
        "mmwave_weight": 1.0,
        "temperature_threshold": 70.0,
        "neighbor_influence": 0.5
    },
    "target_nodes": "all"
}
```

## Status Update Logic

### Local Status Calculation

```c
// In status_manager.cpp
void updateNodeStatus(NodeStatus* status) {
    // Read sensors
    float temp = readTemperature();
    bool proximity = readMmWaveProximity();
    
    // OR operation: status = status OR proximity
    status->active = status->active || proximity;
    status->temperature = temp;
    status->mmwave_proximity = proximity;
    
    // Check if status changed
    if (status->active != previous_status) {
        shareStatusWithNeighbors(status);
        previous_status = status->active;
    }
}
```

### Neighbor Status Propagation

```c
// When a status update is received from a neighbor
void onStatusReceived(StatusUpdate* update) {
    // Update internal neighbor table
    updateNeighborTable(update->mac, update->rssi);
    
    // Trigger local status update
    // This may modify our own status based on the new information
    updateNodeStatus(&local_status);
    
    // Forward to other neighbors if appropriate
    forwardToNeighbors(update);
}
```

### Rule-Based Activation

When MQTT is available, nodes receive rule updates that modify the activation function:

```c
// Default: Simple OR operation
typedef enum {
    ACTIVATION_OR,
    ACTIVATION_AND,
    ACTIVATION_CUSTOM
} ActivationFunction;

// Rule structure
struct ActivationRule {
    ActivationFunction function;
    float temperature_threshold;
    float proximity_weight;
    float neighbor_influence;
};

// Enhanced status update with rules
void updateNodeStatusWithRules(NodeStatus* status, ActivationRule* rule) {
    bool proximity = readMmWaveProximity();
    float temp = readTemperature();
    
    bool temp_condition = (temp > rule->temperature_threshold);
    bool proximity_condition = proximity;
    
    switch (rule->function) {
        case ACTIVATION_OR:
            status->active = status->active || proximity_condition || temp_condition;
            break;
        case ACTIVATION_AND:
            status->active = status->active && proximity_condition && temp_condition;
            break;
        case ACTIVATION_CUSTOM:
            // Custom logic based on weights
            float score = (proximity_condition * rule->proximity_weight) +
                         (temp_condition * (1.0 - rule->proximity_weight));
            status->active = score > 0.5;
            break;
    }
}
```

## Admin Panel Features

The browser-based admin panel (`http://localhost:3000/admin`) provides:

### 📋 Dashboard
- Overview of all connected nodes
- Real-time statistics (temperature range, proximity events, etc.)
- Network health indicators

### ⚙️ Rule Configuration
- **Activation Function**: Choose OR, AND, or custom logic
- **Temperature Threshold**: Set temperature-based activation
- **Proximity Weight**: Adjust influence of mmWave detection
- **Neighbor Influence**: How much neighbor status affects local status

### 🎯 Targeted Commands
- Send commands to specific nodes by MAC address
- Broadcast commands to all nodes
- Group nodes by region or characteristics

### 📊 Monitoring
- Real-time MQTT message log
- Node connection/disconnection events
- Status change history

## Deployment Scenarios

### Scenario 1: Standalone Mesh (No Router)
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Node A       │◄──►│ Node B       │◄──►│ Node C       │
│ - Status     │     │ - Status     │     │ - Status     │
│ - Temp       │     │ - Temp       │     │ - Temp       │
│ - mmWave     │     │ - mmWave     │     │ - mmWave     │
│ - 8 Neighbors│     │ - 8 Neighbors│     │ - 8 Neighbors│
└─────────────┘     └─────────────┘     └─────────────┘
```
- Nodes form mesh using ESP-WiFi-Mesh
- Status sharing happens peer-to-peer
- No external infrastructure required

### Scenario 2: Connected Mesh (With Router)
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Node A       │◄──►│ Node B       │◄──►│ Node C       │
│ - Status     │     │ - Status     │     │ - Status     │
│ - Temp       │     │ - Temp       │     │ - Temp       │
│ - mmWave     │     │ - mmWave     │     │ - mmWave     │
│ - MQTT       │     │ - MQTT       │     │ - MQTT       │
└──────┬───────┘     └──────┬───────┘     └──────┬───────┘
       │                    │                    │
       └────────────────────┴────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ WiFi Router      │
                    └────────┬────────┘
                             │
        ┌────────────────────┼────────────────────┐
        ▼                    ▼                    ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│ MQTT Broker    │ │ Node.js App    │ │ Admin Panel    │
│ (Mosquitto)    │ │ (Visualization)│ │ (Browser)      │
└───────────────┘ └───────────────┘ └───────────────┘
```
- Nodes connect to WiFi router
- MQTT broker runs on connected computer
- Visualization and admin panel provide full control
- Status updates flow both through mesh AND to MQTT

## Scalability Considerations

### Network Size
| Nodes | Mesh Performance | MQTT Performance | Notes |
|-------|------------------|------------------|-------|
| 10-50 | Excellent | Excellent | Ideal for testing |
| 50-200 | Very Good | Good | Recommended for production |
| 200-500 | Good | Moderate | May need MQTT optimizations |
| 500-1000 | Moderate | Needs tuning | Use QoS 0, retain=false |

### Optimization Techniques

1. **MQTT QoS**: Use QoS 0 for high-frequency status updates
2. **Message Throttling**: Limit MQTT publishing rate per node
3. **Topic Hierarchy**: Use efficient topic structures
4. **Payload Compression**: Consider binary payloads for large deployments
5. **Connection Pooling**: Reuse MQTT connections where possible

### Mesh Optimization
- **Beacon Rate**: Adjust based on node density (100ms-1s)
- **Neighbor Count**: 8 provides good balance of connectivity and overhead
- **TX Power**: Auto-adjust based on weakest neighbor RSSI
- **Channel Selection**: Use less congested channels (1, 6, 11 for 2.4GHz)

## Troubleshooting

### Mesh Formation Issues

**Symptom**: Nodes don't form mesh network
- ✅ Verify all nodes have same WiFi channel configured
- ✅ Check nodes are within radio range (test with 2-3 nodes first)
- ✅ Ensure ESP-WiFi-Mesh is enabled in sdkconfig
- ✅ Verify ESP-IDF version is v4.4+
- ✅ Check for WiFi interference from other networks

**Solution**: Start with 2 nodes, verify mesh forms, then add more gradually.

### MQTT Connection Issues

**Symptom**: Nodes don't connect to MQTT broker
- ✅ Verify WiFi credentials are correct (set as build flags)
- ✅ Check MQTT broker is running (`mosquitto` process)
- ✅ Verify broker IP address is correct in config
- ✅ Check firewall allows connections on port 1883
- ✅ Test with `mosquitto_sub -t "#" -v` to see if messages arrive

**Solution**: Test MQTT separately first, then integrate with mesh.

### Status Not Updating

**Symptom**: Status changes don't propagate
- ✅ Verify mmWave sensor is connected and working
- ✅ Check neighbor discovery is functioning (RSSI values)
- ✅ Ensure status update function is called on sensor changes
- ✅ Confirm messages are sent to all 8 neighbors

**Solution**: Enable debug logging for status manager and neighbor manager.

### Visualization Not Showing Nodes

**Symptom**: Nodes don't appear in web interface
- ✅ Verify visualization server is running (`npm start`)
- ✅ Check MQTT broker is receiving node messages
- ✅ Ensure Node.js app is subscribed to correct MQTT topics
- ✅ Verify WebSocket connection is established
- ✅ Check browser console for errors

**Solution**: Use `mosquitto_sub` to verify MQTT messages, then check Node.js logs.

## Debugging

### Enable Debug Logging

In `firmware/main/config.h`:

```c
// Debug flags
#define DEBUG_MESH true
#define DEBUG_NEIGHBOR true
#define DEBUG_STATUS true
#define DEBUG_MQTT true
#define DEBUG_SENSORS true
```

View logs with:
```bash
idf.py monitor
# or
pio device monitor
```

### Useful Commands

```bash
# Monitor MQTT messages
mosquitto_sub -t "mesh/#" -v

# Check Mosquitto logs
tail -f /var/log/mosquitto/mosquitto.log

# Test MQTT publishing
mosquitto_pub -t "mesh/test" -m "hello"

# Check Node.js server logs
cd visualization/app
npm start  # Logs appear in console
```

## Performance Tuning

### For 400-1000 Nodes

**Recommended Settings:**
```c
// In config.h
#define MAX_NEIGHBORS 8           // Optimal for scalability
#define BEACON_INTERVAL_MS 500    // Balance between discovery and overhead
#define STATUS_UPDATE_INTERVAL_MS 1000  // 1Hz status updates
#define MQTT_PUBLISH_INTERVAL_MS 5000   // Throttle MQTT to 0.2Hz
```

**MQTT Configuration (mosquitto.conf):**
```
max_connections 2000
max_inflight_messages 20
message_size_limit 1024
persistent_client_expiration 1d
```

## Security Considerations

### Current Implementation
- Mesh communication: ESP-WiFi-Mesh (proprietary, not encrypted by default)
- MQTT: Plaintext (no TLS by default)
- No node authentication

### Recommended Enhancements

1. **MQTT Security:**
   - Enable TLS: `MQTT_BROKER_IP` → `mqtts://broker:8883`
   - Use client certificates for node authentication
   - Set up ACLs in Mosquitto

2. **Mesh Security:**
   - Enable ESP-WiFi-Mesh encryption (if available in your ESP-IDF version)
   - Implement application-level encryption for status messages

3. **Network Security:**
   - Use a dedicated VLAN for the mesh network
   - Implement firewall rules to restrict access
   - Use strong WiFi passwords

4. **Admin Security:**
   - Add authentication to admin panel
   - Use HTTPS for web interface
   - Implement rate limiting for MQTT commands

## Contributing

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Commit your changes**: `git commit -m 'Add amazing feature'`
4. **Push to the branch**: `git push origin feature/amazing-feature`
5. **Open a Pull Request**

### Development Setup

```bash
# Install dependencies
git submodule update --init --recursive
cd firmware
pio run  # Test build

cd ../visualization/app
npm install
npm run dev  # Development mode with hot reload
```

## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- **[Espressif Systems](https://www.espressif.com/)** for ESP32C3 and ESP-WiFi-Mesh
- **[Eclipse Mosquitto](https://mosquitto.org/)** for MQTT broker
- **[Node.js](https://nodejs.org/)** for backend runtime
- **[D3.js](https://d3js.org/)** for visualization
- **[PlatformIO](https://platformio.org/)** for cross-platform development

## Contact & Support

For questions, issues, or contributions:
- Open an issue on [GitHub](https://github.com/n2048-creative-technology/mesh-visualization-)
- Check the [documentation](docs/)
- Review the [architecture](docs/architecture.md) for system details

---

**Built for large-scale adaptive mesh networks with 💙 by n2048 Creative Technology**

*Designed for 400-1000 ESP32C3 nodes with real-time visualization and MQTT control*
