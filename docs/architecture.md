# Mesh Network Architecture

## Overview

This document describes the architecture of the **Adaptive ESP32 Mesh Network with Real-Time Topology Visualization**. The system consists of:

1. **ESP32 Mesh Nodes** - Decentralized nodes that form an adaptive mesh network
2. **Visualization Server** - Node.js backend that receives UDP messages and serves web clients
3. **Web Frontend** - D3.js-based real-time visualization of the mesh topology

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Web Browser (D3.js)                          │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────┐  │
│  │  Node Rendering  │    │  Link Rendering  │    │   Controls   │  │
│  └─────────────────┘    └─────────────────┘    └─────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │ WebSocket
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Visualization Server                          │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────┐  │
│  │   UDP Listener  │    │ WebSocket Server │    │  HTTP API    │  │
│  │   (Port 1234)   │    │   (Port 3000)    │    │  (/api/*)    │  │
│  └─────────────────┘    └─────────────────┘    └─────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │ UDP (Mesh Forwarding)
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                       ESP32 Mesh Network                            │
│                                                                     │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐            │
│  │   Node A    │◄──►│   Node B    │◄──►│   Node C    │            │
│  │             │    │             │    │             │            │
│  │ - Beacon    │    │ - Beacon    │    │ - Beacon    │            │
│  │   Monitor   │    │   Monitor   │    │   Monitor   │            │
│  │ - Neighbor  │    │ - Neighbor  │    │ - Neighbor  │            │
│  │   List      │    │   List      │    │   List      │            │
│  │ - UDP       │    │ - UDP       │    │ - UDP       │            │
│  │   Unicast   │    │   Unicast   │    │   Unicast   │            │
│  │ - TX Power  │    │ - TX Power  │    │ - TX Power  │            │
│  │   Adjust    │    │   Adjust    │    │   Adjust    │            │
│  └─────────────┘    └─────────────┘    └─────────────┘            │
│                                                                     │
└─────────────────────────────────────────────────────────────────┘
```

## Component Details

### 1. ESP32 Mesh Node

Each ESP32 node runs the following components:

#### Mesh Networking Layer
- **ESP-WiFi-Mesh**: Provides the underlying mesh networking capability
- **Flat Topology**: No root node, all nodes are equal
- **Self-Healing**: Automatic reconnection when nodes fail or move
- **Max Hops**: 10 (configurable)

#### Beacon Monitoring
- **Promiscuous Mode**: Captures all WiFi frames, including beacons
- **RSSI Tracking**: Measures signal strength from neighboring nodes
- **Neighbor List**: Maintains top 8 closest neighbors by RSSI
- **Dynamic Updates**: Updates neighbor list in real-time based on RSSI changes

#### Communication
- **UDP Unicast**: Sends state updates only to the 8 closest neighbors
- **Mesh Forwarding**: Forwards non-neighbor packets through the mesh
- **Binary Protocol**: Efficient binary message format (77 bytes)
- **Checksum**: Simple checksum for message integrity

#### State Management
- **Node State**: Tracks node state (idle, active, error, booting)
- **LED Control**: RGB LED indicates node state
- **Sensor Data**: Temperature and mmWave sensor readings
- **Dynamic TX Power**: Adjusts transmission power based on weakest neighbor RSSI

#### Message Format

```c
struct NodeUpdate {
    uint8_t version;         // Protocol version (1 byte)
    uint8_t msg_type;        // Message type (1 byte)
    uint8_t mac[6];          // Node MAC address (6 bytes)
    uint8_t state;           // Node state (1 byte)
    uint8_t color[3];        // RGB color (3 bytes)
    int16_t temperature;     // Temperature ×10 (2 bytes)
    uint8_t mmwave_presence; // Presence detection (1 byte)
    uint32_t mmwave_distance; // Distance in mm (4 bytes)
    uint32_t timestamp;      // Unix timestamp (4 bytes)
    struct {
        uint8_t mac[6];      // Neighbor MAC (6 bytes)
        int8_t rssi;         // RSSI (1 byte)
    } neighbors[8];          // Top 8 neighbors (56 bytes)
    uint16_t checksum;       // Checksum (2 bytes)
};
// Total: 77 bytes
```

### 2. Visualization Server

The Node.js server provides:

#### UDP Listener
- Listens on port 1234 for binary UDP messages
- Parses NodeUpdate messages
- Stores node states and neighbor lists
- Forwards updates to WebSocket clients

#### WebSocket Server
- Broadcasts real-time updates to web clients
- Sends full state to new clients on connection
- Provides periodic statistics updates

#### HTTP API
- `/api/nodes`: Returns all node states (JSON)
- `/api/topology`: Returns mesh topology with connections and RSSI
- `/api/stats`: Returns server statistics

#### Data Storage
- In-memory storage of node states
- Automatic cleanup of stale nodes (10 second timeout)
- Message statistics tracking

### 3. Web Frontend

The D3.js-based frontend provides:

#### Visualization
- **Nodes**: Circles representing ESP32 nodes
- **Connections**: Lines between neighbors, colored by RSSI strength
- **Force-Directed Layout**: Nodes repel each other, connections pull nodes together
- **Relative Positioning**: Nodes are positioned based on mesh topology

#### Features
- **Real-Time Updates**: 10 Hz updates via WebSocket
- **Multiple Color Schemes**: By RSSI, state, temperature, or fixed
- **Interactive Controls**: Toggle labels, RSSI display, filter by RSSI
- **Node Information Panel**: Detailed info on click
- **Responsive Design**: Works on desktop and mobile

#### Color Coding
- **By RSSI**: Green (strong) to Red (weak)
- **By State**: Blue (idle), Green (active), Red (error), Yellow (booting)
- **By Temperature**: Blue (cold) to Red (hot)

## Communication Flow

### 1. Node Discovery

```
1. Node A enables promiscuous mode
2. Node A receives beacon frames from all nearby nodes
3. Node A extracts MAC address and RSSI from each beacon
4. Node A maintains sorted list of top 8 neighbors by RSSI
5. Node A updates neighbor list when RSSI changes >3 dBm or timeout occurs
```

### 2. State Updates

```
1. Node A reads sensors (temperature, mmWave)
2. Node A prepares NodeUpdate message with current state
3. Node A sends UDP unicast to each of its 8 neighbors
4. Neighbors receive and process the update
5. Non-neighbor nodes forward the message via mesh
6. Visualization server receives the message (directly or forwarded)
```

### 3. TX Power Adjustment

```
1. Node A monitors RSSI of all neighbors
2. Node A finds weakest neighbor RSSI
3. Node A calculates required TX power to maintain target RSSI (-65 dBm)
4. Node A adjusts TX power in steps (2 dBm at a time)
5. Node A sets new TX power using esp_wifi_set_max_tx_power()
```

### 4. Visualization Updates

```
1. Visualization server receives UDP message
2. Server parses binary message and updates node data
3. Server broadcasts update to all WebSocket clients
4. Web clients receive update and update D3.js visualization
5. Force simulation updates node positions
6. Links and nodes are redrawn with new colors/positions
```

## Adaptive Features

### Dynamic Neighbor Management
- **Addition**: New nodes are added to neighbor list when detected
- **Removal**: Nodes are removed after 5 seconds of inactivity
- **Reordering**: Neighbor list is sorted by RSSI (strongest first)
- **Threshold**: Updates triggered when RSSI changes by >3 dBm

### Dynamic TX Power
- **Target RSSI**: -65 dBm at weakest neighbor
- **Adjustment Range**: 0 dBm to 20 dBm (ESP32 maximum)
- **Step Size**: 2 dBm per adjustment
- **Frequency**: Adjusted on each state update (10 Hz)

### Mesh Forwarding
- **Non-Neighbor Packets**: Forwarded through mesh to visualization
- **No Processing**: Non-neighbor messages are only forwarded, not processed
- **Efficient**: Uses ESP-WiFi-Mesh's built-in forwarding capabilities

### Self-Healing
- **Vote Percentage**: 1% (aggressive self-healing)
- **Max Hops**: 10 (messages can traverse up to 10 nodes)
- **Automatic Reconnection**: Nodes automatically reconnect when parent fails

## Performance Considerations

### Message Size
- Binary protocol: 77 bytes per message
- 10 Hz update rate: 770 bytes/second per node
- 400 nodes: ~308 KB/second total (worst case)

### Network Efficiency
- UDP unicast to 8 neighbors only (not broadcast)
- Mesh forwarding for non-neighbor messages
- Checksum verification to filter corrupted messages

### Visualization Performance
- D3.js force simulation optimized for real-time updates
- WebSocket for efficient real-time communication
- Client-side filtering (min RSSI) to reduce rendering load

## Security Considerations

### WiFi Security
- WPA2-Enterprise recommended for mesh network
- Unique credentials for each deployment

### Message Integrity
- Simple checksum to detect corrupted messages
- Protocol version for backward compatibility

### Network Isolation
- Separate network for mesh nodes
- Firewall rules to protect visualization server

## Scalability

### Node Count
- **Tested**: 2-20 nodes
- **Target**: 400+ nodes
- **Limitations**: 
  - UDP port exhaustion (65,535 ports)
  - Mesh network overhead
  - Visualization rendering performance

### Optimization Strategies
1. **Message Throttling**: Limit updates to 10 Hz maximum
2. **Binary Protocol**: Minimize message size
3. **Client-Side Filtering**: Filter by RSSI before rendering
4. **Level of Detail**: Reduce detail for distant nodes
5. **Clustering**: Group nearby nodes when count is high

## Deployment Architecture

### Small Deployment (2-10 nodes)
```
┌─────────┐    ┌─────────┐    ┌─────────┐
│  Node 1  │◄──►│  Node 2  │◄──►│  Node 3  │
└─────────┘    └─────────┘    └─────────┘
       │              │              │
       └──────────────┴──────────────┘
                    │ UDP
                    ▼
            ┌─────────────┐
            │  Laptop/RPi  │
            │  (Visualization) │
            └─────────────┘
```

### Large Deployment (100+ nodes)
```
┌─────────┐    ┌─────────┐    ┌─────────┐
│  Node 1  │◄──►│  Node 2  │◄──►│  Node 3  │
└─────────┘    └─────────┘    └─────────┘
       │              │              │
       ▼              ▼              ▼
┌─────────────────────────────────────────────┐
│               Mesh Network                     │
│  (Multiple hops, self-healing, forwarding)    │
└─────────────────────────────────────────────┘
                       │
                       ▼
                ┌─────────────┐
                │  Gateway     │
                │  Node        │
                └─────────────┘
                       │
                       ▼
                ┌─────────────┐
                │  Laptop/RPi  │
                │  (Visualization) │
                └─────────────┘
```

## Future Enhancements

1. **RSSI-Based Trilateration**: Estimate physical positions from RSSI
2. **Path Finding**: Visualize optimal paths between nodes
3. **Historical Data**: Store and replay network state over time
4. **Alerts**: Notify when nodes go offline or RSSI drops
5. **3D Visualization**: Use Three.js for 3D rendering
6. **Mobile App**: Native mobile visualization
7. **Configuration UI**: Web-based node configuration
8. **Firmware Update**: Over-the-air firmware updates
9. **Security**: Encrypted messages and authentication
10. **Analytics**: Network performance metrics and insights
