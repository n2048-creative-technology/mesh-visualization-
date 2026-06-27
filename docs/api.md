# API Documentation

## Overview

This document describes the message formats, APIs, and protocols used in the Adaptive ESP32 Mesh Network system.

## Message Formats

### Binary UDP Message Format

All communication between ESP32 nodes and the visualization server uses a binary protocol for efficiency.

#### NodeUpdate Message (77 bytes)

```c
// Message structure
struct NodeUpdate {
    // Header (2 bytes)
    uint8_t version;         // Protocol version (0x01)
    uint8_t msg_type;        // Message type (0x01 = state update)
    
    // Node identification (6 bytes)
    uint8_t mac[6];          // Source node MAC address
    
    // Node state (13 bytes)
    uint8_t state;           // Node state (0=idle, 1=active, 2=error, 3=booting)
    uint8_t color[3];        // RGB color values (0-255)
    int16_t temperature;     // Temperature × 10 (e.g., 250 = 25.0°C)
    uint8_t mmwave_presence; // 0 = no presence, 1 = presence detected
    uint32_t mmwave_distance; // Distance in millimeters
    uint32_t timestamp;      // Unix timestamp (seconds since epoch)
    
    // Neighbor list (56 bytes)
    struct {
        uint8_t mac[6];      // Neighbor MAC address
        int8_t rssi;         // RSSI in dBm (-127 to 0)
    } neighbors[8];          // Top 8 neighbors
    
    // Footer (2 bytes)
    uint16_t checksum;       // Simple checksum of all preceding bytes
};
```

#### Message Types

| Type Value | Type Name | Description |
|------------|-----------|-------------|
| 0x01 | MSG_TYPE_STATE_UPDATE | Regular state update from node |
| 0x02 | MSG_TYPE_BEACON | Beacon frame information |
| 0x03 | MSG_TYPE_FORWARD | Forwarded message (non-neighbor) |

#### Protocol Version

- **Current Version**: 0x01
- **Backward Compatibility**: Messages with unsupported versions are ignored

#### Checksum Calculation

The checksum is a simple sum of all bytes in the message (excluding the checksum field itself):

```c
uint16_t calculate_checksum(mesh_message_t *msg) {
    uint16_t checksum = 0;
    uint8_t *data = (uint8_t *)msg;
    size_t len = sizeof(mesh_message_t) - sizeof(msg->checksum);
    
    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    
    return checksum;
}
```

### JSON Message Format (WebSocket)

The visualization server broadcasts messages to web clients using JSON format over WebSocket.

#### Full State Message

Sent to new clients on connection:

```json
{
    "type": "full_state",
    "data": [
        {
            "version": 1,
            "msgType": 1,
            "mac": "aa:bb:cc:dd:ee:ff",
            "state": 1,
            "color": {"r": 0, "g": 255, "b": 0},
            "temperature": 25.0,
            "mmwavePresence": true,
            "mmwaveDistance": 1500,
            "timestamp": 1719500000,
            "neighbors": [
                {"mac": "11:22:33:44:55:66", "rssi": -65},
                {"mac": "aa:bb:cc:dd:ee:01", "rssi": -70}
            ],
            "receivedAt": 1719500001000
        }
    ]
}
```

#### Node Update Message

Sent when a node updates its state:

```json
{
    "type": "node_update",
    "data": {
        "version": 1,
        "msgType": 1,
        "mac": "aa:bb:cc:dd:ee:ff",
        "state": 1,
        "color": {"r": 0, "g": 255, "b": 0},
        "temperature": 25.0,
        "mmwavePresence": true,
        "mmwaveDistance": 1500,
        "timestamp": 1719500000,
        "neighbors": [
            {"mac": "11:22:33:44:55:66", "rssi": -65},
            {"mac": "aa:bb:cc:dd:ee:01", "rssi": -70}
        ],
        "receivedAt": 1719500001000
    }
}
```

#### Statistics Message

Sent periodically to all clients:

```json
{
    "type": "stats",
    "data": {
        "nodeCount": 42,
        "messageCount": 12345,
        "lastMessageTime": 1719500001000
    }
}
```

## HTTP API

The visualization server provides a REST API for querying network state.

### Base URL

```
http://<server-ip>:3000/api/
```

### Endpoints

#### GET /api/nodes

Returns an array of all nodes with their current state.

**Request:**
```
GET /api/nodes
```

**Response:**
```json
[
    {
        "version": 1,
        "msgType": 1,
        "mac": "aa:bb:cc:dd:ee:ff",
        "state": 1,
        "color": {"r": 0, "g": 255, "b": 0},
        "temperature": 25.0,
        "mmwavePresence": true,
        "mmwaveDistance": 1500,
        "timestamp": 1719500000,
        "neighbors": [
            {"mac": "11:22:33:44:55:66", "rssi": -65},
            {"mac": "aa:bb:cc:dd:ee:01", "rssi": -70}
        ],
        "receivedAt": 1719500001000
    },
    {
        "version": 1,
        "msgType": 1,
        "mac": "11:22:33:44:55:66",
        "state": 1,
        "color": {"r": 255, "g": 0, "b": 0},
        "temperature": 26.5,
        "mmwavePresence": false,
        "mmwaveDistance": 0,
        "timestamp": 1719500000,
        "neighbors": [
            {"mac": "aa:bb:cc:dd:ee:ff", "rssi": -65},
            {"mac": "11:22:33:44:55:77", "rssi": -75}
        ],
        "receivedAt": 1719500001000
    }
]
```

**Status Codes:**
- 200 OK: Returns array of nodes

#### GET /api/topology

Returns the mesh topology as a graph structure.

**Request:**
```
GET /api/topology
```

**Response:**
```json
{
    "aa:bb:cc:dd:ee:ff": {
        "neighbors": ["11:22:33:44:55:66", "aa:bb:cc:dd:ee:01"],
        "rssi": [-65, -70],
        "state": 1,
        "color": {"r": 0, "g": 255, "b": 0},
        "temperature": 25.0,
        "mmwavePresence": true,
        "mmwaveDistance": 1500
    },
    "11:22:33:44:55:66": {
        "neighbors": ["aa:bb:cc:dd:ee:ff", "11:22:33:44:55:77"],
        "rssi": [-65, -75],
        "state": 1,
        "color": {"r": 255, "g": 0, "b": 0},
        "temperature": 26.5,
        "mmwavePresence": false,
        "mmwaveDistance": 0
    }
}
```

**Status Codes:**
- 200 OK: Returns topology object

#### GET /api/stats

Returns server statistics.

**Request:**
```
GET /api/stats
```

**Response:**
```json
{
    "nodeCount": 42,
    "messageCount": 12345,
    "lastMessageTime": 1719500001000,
    "uptime": 3600.5
}
```

**Status Codes:**
- 200 OK: Returns statistics object

## UDP API

### Server Configuration

- **IP Address**: Configurable (default: 192.168.1.254)
- **Port**: 1234
- **Protocol**: UDP

### Message Flow

1. **ESP32 Node → Server**: Nodes send NodeUpdate messages to the server
2. **Server → Web Clients**: Server broadcasts updates via WebSocket
3. **Node → Node**: Nodes send UDP unicast to their 8 closest neighbors
4. **Node → Node (Forwarding)**: Non-neighbor messages are forwarded through the mesh

### IP Address Assignment

Nodes use static IP assignment based on their MAC address:

```
IP: 192.168.1.<last_byte_of_mac>
```

For example:
- MAC: `aa:bb:cc:dd:ee:01` → IP: `192.168.1.1`
- MAC: `aa:bb:cc:dd:ee:ff` → IP: `192.168.1.255`

### Message Forwarding

When a node receives a message from a non-neighbor:

1. The node checks if the message is from a neighbor
2. If not, the node forwards the message via ESP-WiFi-Mesh
3. The message eventually reaches the visualization server

## WebSocket API

### Connection

- **URL**: `ws://<server-ip>:3000`
- **Protocol**: WebSocket

### Message Types

#### Client → Server

No messages are sent from client to server in the current implementation.

#### Server → Client

1. **Full State** (`full_state`): Sent on connection, contains all node data
2. **Node Update** (`node_update`): Sent when a node updates its state
3. **Statistics** (`stats`): Sent periodically with server statistics

### Example Connection Flow

```
Client: Connect to ws://localhost:3000
Server: → {"type": "full_state", "data": [...]}
Server: → {"type": "node_update", "data": {...}}
Server: → {"type": "node_update", "data": {...}}
Server: → {"type": "stats", "data": {...}}
```

## Node State Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | NODE_STATE_IDLE | Node is idle, waiting for activity |
| 1 | NODE_STATE_ACTIVE | Node is active and operating normally |
| 2 | NODE_STATE_ERROR | Node has encountered an error |
| 3 | NODE_STATE_BOOTING | Node is booting up |

## Configuration Parameters

### ESP32 Node Configuration (config.h)

```c
// WiFi and Mesh
#define WIFI_CHANNEL          36      // 5 GHz channel
#define MESH_MAX_HOPS          10
#define MESH_VOTE_PERCENT      1       // Self-healing percentage

// UDP
#define UDP_PORT              1234
#define VISUALIZATION_IP      "192.168.1.254"

// Neighbor Settings
#define MAX_NEIGHBORS          8
#define RSSI_THRESHOLD         3       // dBm change to trigger update
#define NEIGHBOR_TIMEOUT_MS    5000    // Remove after 5s of inactivity

// TX Power
#define TARGET_RSSI           -65      // Target RSSI at weakest neighbor
#define MIN_TX_POWER          0       // dBm
#define MAX_TX_POWER          20      // dBm
#define TX_POWER_STEP         2       // dBm adjustment step

// State Updates
#define STATE_UPDATE_INTERVAL 100     // ms (10 Hz)
```

### Visualization Server Configuration (server.js)

```javascript
const UDP_PORT = 1234;
const HTTP_PORT = 3000;
const WS_PORT = 3000;
```

## Error Handling

### Message Validation

1. **Checksum**: All messages must have valid checksums
2. **Version**: Messages with unsupported versions are ignored
3. **Size**: Messages shorter than expected are ignored
4. **MAC Address**: Invalid MAC addresses are filtered out

### Network Errors

1. **UDP Socket Errors**: Logged and socket is recreated
2. **WebSocket Disconnections**: Automatic reconnection after 5 seconds
3. **HTTP Server Errors**: Logged and server continues running

### Node Errors

1. **Neighbor Timeout**: Nodes not seen for 5 seconds are removed from neighbor list
2. **RSSI Threshold**: Neighbor list updates only when RSSI changes by >3 dBm
3. **TX Power Limits**: TX power is clamped to valid range (0-20 dBm)

## Performance Considerations

### Message Rates

- **State Updates**: 10 Hz per node (configurable)
- **Beacon Monitoring**: Continuous (100 ms interval)
- **Neighbor Cleanup**: Every 5 seconds
- **WebSocket Updates**: As fast as messages are received

### Bandwidth Usage

- **Per Node**: 77 bytes × 10 updates/second = 770 bytes/second
- **400 Nodes**: 770 × 400 = 308,000 bytes/second ≈ 308 KB/second

### Optimization Techniques

1. **Binary Protocol**: Minimizes message size
2. **UDP Unicast**: Only sends to 8 closest neighbors
3. **Checksum**: Simple and fast verification
4. **Client-Side Filtering**: Reduces rendering load
5. **Force Simulation**: Optimized for real-time updates

## Security Considerations

### Current Implementation

- **No Encryption**: Messages are sent in plaintext
- **No Authentication**: Any node can send messages
- **Checksum Only**: Simple integrity check

### Recommended Enhancements

1. **Message Encryption**: AES-128 for message payloads
2. **Node Authentication**: Shared secret or certificate-based
3. **Message Signing**: HMAC for message integrity
4. **Network Isolation**: Separate VLAN for mesh nodes
5. **Firewall Rules**: Restrict access to visualization server

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 0x01 | 2024-06-27 | Initial version |

## Compatibility

- **ESP-IDF**: v4.4+
- **ESP32 Models**: ESP32-C5 (recommended for 5 GHz), ESP32, ESP32-S3
- **Node.js**: v16+
- **Browsers**: Modern browsers with WebSocket and D3.js v7 support
