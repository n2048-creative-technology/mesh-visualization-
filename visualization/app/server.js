/**
 * Mesh Visualization Server
 * Node.js backend for UDP listener, MQTT client and WebSocket bridge
 * Receives binary UDP messages and MQTT messages from ESP32 nodes and broadcasts to web clients
 */

const dgram = require('dgram');
const WebSocket = require('ws');
const express = require('express');
const path = require('path');
const mqtt = require('mqtt');

// ============================================================================
// Configuration
// ============================================================================

const UDP_PORT = 1234;
const HTTP_PORT = 3000;
const WS_PORT = 3000;

// ============================================================================
// MQTT Configuration
// ============================================================================

// Read MQTT broker URL from environment or use default
// Note: To work with the firmware, this should match the IP in config.h (10.64.5.196)
// But your current machine IP is 10.65.5.196. Use environment variable to override:
// MQTT_BROKER_URL=mqtt://10.65.5.196:1883 node server.js
const MQTT_BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883';
const MQTT_TOPOLOGY_TOPIC = process.env.MQTT_TOPOLOGY_TOPIC || 'mesh/topology';
const MQTT_STATE_TOPIC = process.env.MQTT_STATE_TOPIC || 'mesh/state';
const MQTT_CLIENT_ID = process.env.MQTT_CLIENT_ID || 'mesh_visualization_server';

// Log MQTT configuration at startup
console.log('MQTT Configuration:');
console.log('  Broker URL:', MQTT_BROKER_URL);
console.log('  Topology Topic:', MQTT_TOPOLOGY_TOPIC);
console.log('  State Topic:', MQTT_STATE_TOPIC);

// MQTT Client
let mqttClient = null;
let mqttConnected = false;

// ============================================================================
// Data Structures
// ============================================================================

// Node state storage
const nodes = {};

// Message statistics
let messageCount = 0;
let lastMessageTime = Date.now();

// ============================================================================
// UDP Message Parser
// ============================================================================

/**
 * Protocol version and message types
 */
const PROTOCOL_VERSION = 0x01;
const MSG_TYPE_STATE_UPDATE = 0x01;
const MSG_TYPE_BEACON = 0x02;
const MSG_TYPE_FORWARD = 0x03;

/**
 * Parse binary NodeUpdate message
 * Message format:
 * - version (1 byte)
 * - msg_type (1 byte)
 * - mac (6 bytes)
 * - state (1 byte)
 * - color (3 bytes)
 * - temperature (2 bytes, int16_t)
 * - mmwave_presence (1 byte)
 * - mmwave_distance (4 bytes, uint32_t)
 * - timestamp (4 bytes, uint32_t)
 * - neighbors (8 x 7 bytes: mac[6] + rssi[1])
 * - checksum (2 bytes)
 */
function parseNodeUpdate(buffer) {
    if (buffer.length < 77) {
        console.warn(`Short message received: ${buffer.length} bytes`);
        return null;
    }
    
    let offset = 0;
    
    // Read version
    const version = buffer.readUInt8(offset);
    offset += 1;
    
    if (version !== PROTOCOL_VERSION) {
        console.warn(`Unsupported protocol version: ${version}`);
        return null;
    }
    
    // Read message type
    const msgType = buffer.readUInt8(offset);
    offset += 1;
    
    // Read MAC address
    const mac = buffer.slice(offset, offset + 6).toString('hex').match(/.{1,2}/g).join(':');
    offset += 6;
    
    // Read node state
    const state = buffer.readUInt8(offset);
    offset += 1;
    
    // Read RGB color
    const color = {
        r: buffer.readUInt8(offset),
        g: buffer.readUInt8(offset + 1),
        b: buffer.readUInt8(offset + 2)
    };
    offset += 3;
    
    // Read temperature (stored as int16_t * 10)
    const temperature = buffer.readInt16LE(offset) / 10.0;
    offset += 2;
    
    // Read mmWave sensor data
    const mmwavePresence = buffer.readUInt8(offset);
    offset += 1;
    
    const mmwaveDistance = buffer.readUInt32LE(offset);
    offset += 4;
    
    // Read timestamp
    const timestamp = buffer.readUInt32LE(offset);
    offset += 4;
    
    // Read neighbors
    const neighbors = [];
    for (let i = 0; i < 8; i++) {
        const neighborMac = buffer.slice(offset, offset + 6).toString('hex').match(/.{1,2}/g).join(':');
        offset += 6;
        const neighborRssi = buffer.readInt8(offset);
        offset += 1;
        
        if (neighborMac !== '00:00:00:00:00:00' && neighborRssi !== -127) {
            neighbors.push({
                mac: neighborMac,
                rssi: neighborRssi
            });
        }
    }
    
    // Read checksum (last 2 bytes)
    const receivedChecksum = buffer.readUInt16LE(offset);
    
    // Verify checksum
    const calculatedChecksum = calculateChecksum(buffer.slice(0, offset));
    if (receivedChecksum !== calculatedChecksum) {
        console.warn(`Checksum mismatch for ${mac}: received=${receivedChecksum}, calculated=${calculatedChecksum}`);
        return null;
    }
    
    return {
        version,
        msgType,
        mac,
        state,
        color,
        temperature,
        mmwavePresence: mmwavePresence !== 0,
        mmwaveDistance,
        timestamp,
        neighbors,
        receivedAt: Date.now()
    };
}

/**
 * Calculate checksum for message (sum of all bytes except last 2)
 */
function calculateChecksum(buffer) {
    let checksum = 0;
    for (let i = 0; i < buffer.length; i++) {
        checksum += buffer.readUInt8(i);
    }
    return checksum & 0xFFFF;
}

// ============================================================================
// MQTT Message Parser
// ============================================================================

/**
 * Convert MQTT topology message to node update format
 * MQTT topology message format:
 * {
 *   "node_mac": "xx:xx:xx:xx:xx:xx",
 *   "node_state": 0-3,
 *   "layer": number,
 *   "routing_table_size": number,
 *   "neighbors": [
 *     {"mac": "xx:xx:xx:xx:xx:xx", "rssi": -50, "last_seen": timestamp}
 *   ],
 *   "neighbor_count": number
 * }
 */
function convertMqttTopologyToNode(topologyMsg) {
    const node = {
        version: PROTOCOL_VERSION,
        msgType: MSG_TYPE_STATE_UPDATE,
        mac: topologyMsg.node_mac,
        state: topologyMsg.node_state || 1, // default to idle
        color: { r: 0, g: 255, b: 0 }, // default green
        temperature: 0, // will be updated from state message
        mmwavePresence: false,
        mmwaveDistance: 0,
        timestamp: Date.now() / 1000, // convert to seconds
        neighbors: [],
        receivedAt: Date.now()
    };

    // Convert neighbors from MQTT format to internal format
    if (topologyMsg.neighbors && Array.isArray(topologyMsg.neighbors)) {
        for (const neighbor of topologyMsg.neighbors) {
            if (neighbor.mac && neighbor.rssi !== undefined) {
                node.neighbors.push({
                    mac: neighbor.mac,
                    rssi: neighbor.rssi
                });
            }
        }
    }

    return node;
}

/**
 * Convert MQTT state message to node update format
 * MQTT state message format:
 * {
 *   "mac": "xx:xx:xx:xx:xx:xx",
 *   "state": 0-3,
 *   "temperature": 250 (×10), 
 *   "mmwave_presence": 0 or 1,
 *   "mmwave_distance": 1500,
 *   "timestamp": 123456789,
 *   "color": [255, 0, 0]
 * }
 */
function convertMqttStateToNode(stateMsg) {
    return {
        version: PROTOCOL_VERSION,
        msgType: MSG_TYPE_STATE_UPDATE,
        mac: stateMsg.mac,
        state: stateMsg.state || 1,
        color: {
            r: stateMsg.color && Array.isArray(stateMsg.color) && stateMsg.color[0] !== undefined ? stateMsg.color[0] : 0,
            g: stateMsg.color && Array.isArray(stateMsg.color) && stateMsg.color[1] !== undefined ? stateMsg.color[1] : 255,
            b: stateMsg.color && Array.isArray(stateMsg.color) && stateMsg.color[2] !== undefined ? stateMsg.color[2] : 0
        },
        temperature: (stateMsg.temperature !== undefined ? stateMsg.temperature : 0) / 10.0,
        mmwavePresence: (stateMsg.mmwave_presence || 0) !== 0,
        mmwaveDistance: stateMsg.mmwave_distance || 0,
        timestamp: stateMsg.timestamp || Math.floor(Date.now() / 1000),
        neighbors: [], // will be populated from topology message
        receivedAt: Date.now()
    };
}

// ============================================================================
// MQTT Client Setup
// ============================================================================

/**
 * Connect to MQTT broker
 */
function connectMQTT() {
    try {
        console.log(`Connecting to MQTT broker at ${MQTT_BROKER_URL}...`);
        
        mqttClient = mqtt.connect(MQTT_BROKER_URL, {
            clientId: MQTT_CLIENT_ID,
            clean: true,
            reconnectPeriod: 5000,
            connectTimeout: 10000
        });

        mqttClient.on('connect', () => {
            mqttConnected = true;
            console.log('MQTT connected');
            
            // Subscribe to MQTT topics
            mqttClient.subscribe(MQTT_TOPOLOGY_TOPIC, { qos: 0 }, (err) => {
                if (err) {
                    console.error('Failed to subscribe to topology topic:', err);
                } else {
                    console.log(`Subscribed to ${MQTT_TOPOLOGY_TOPIC}`);
                }
            });

            mqttClient.subscribe(MQTT_STATE_TOPIC, { qos: 0 }, (err) => {
                if (err) {
                    console.error('Failed to subscribe to state topic:', err);
                } else {
                    console.log(`Subscribed to ${MQTT_STATE_TOPIC}`);
                }
            });
        });

        mqttClient.on('message', (topic, message) => {
            try {
                const payload = JSON.parse(message.toString());
                handleMqttMessage(topic, payload);
            } catch (error) {
                console.error('Error parsing MQTT message:', error);
            }
        });

        mqttClient.on('error', (err) => {
            mqttConnected = false;
            console.error('MQTT error:', err);
        });

        mqttClient.on('close', () => {
            mqttConnected = false;
            console.log('MQTT connection closed');
            // Attempt to reconnect
            setTimeout(connectMQTT, 5000);
        });

        mqttClient.on('offline', () => {
            mqttConnected = false;
            console.log('MQTT client offline');
        });

        mqttClient.on('reconnect', () => {
            console.log('MQTT reconnecting...');
        });

    } catch (error) {
        console.error('Failed to initialize MQTT client:', error);
        mqttConnected = false;
    }
}

/**
 * Handle incoming MQTT message
 */
function handleMqttMessage(topic, payload) {
    messageCount++;
    lastMessageTime = Date.now();

    let node = null;

    switch (topic) {
        case MQTT_TOPOLOGY_TOPIC:
            // Handle topology message - this contains neighbor information
            node = convertMqttTopologyToNode(payload);
            break;

        case MQTT_STATE_TOPIC:
            // Handle state message - this contains node sensor data
            node = convertMqttStateToNode(payload);
            break;

        default:
            console.log(`Received MQTT message on unknown topic: ${topic}`);
            return;
    }

    if (node === null || !node.mac) {
        console.warn('Failed to parse MQTT message:', payload);
        return;
    }

    // Update node data
    const existingNode = nodes[node.mac];
    
    if (existingNode) {
        // Merge the new data with existing node
        if (topic === MQTT_TOPOLOGY_TOPIC) {
            // Update neighbors from topology message
            existingNode.neighbors = node.neighbors;
            existingNode.state = node.state;
        } else if (topic === MQTT_STATE_TOPIC) {
            // Update sensor data from state message
            existingNode.state = node.state;
            existingNode.color = node.color;
            existingNode.temperature = node.temperature;
            existingNode.mmwavePresence = node.mmwavePresence;
            existingNode.mmwaveDistance = node.mmwaveDistance;
            existingNode.timestamp = node.timestamp;
        }
        existingNode.receivedAt = Date.now();
        node = existingNode;
    } else {
        // Create new node
        nodes[node.mac] = node;
    }

    // Broadcast to all WebSocket clients
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({ 
                type: 'node_update', 
                data: node 
            }));
        }
    });

    console.log(`MQTT: Received ${topic} from ${node.mac} (Neighbors: ${node.neighbors.length}, Temp: ${node.temperature}°C)`);
}

/**
 * Disconnect MQTT client
 */
function disconnectMQTT() {
    if (mqttClient) {
        try {
            mqttClient.end(true);
            mqttClient = null;
            mqttConnected = false;
            console.log('MQTT client disconnected');
        } catch (error) {
            console.error('Error disconnecting MQTT client:', error);
        }
    }
}

// ============================================================================
// Server Setup
// ============================================================================

// Create Express app
const app = express();
const server = require('http').createServer(app);

// Serve static files
app.use(express.static(path.join(__dirname, 'public')));

// API endpoints
app.get('/api/nodes', (req, res) => {
    res.json(Object.values(nodes));
});

app.get('/api/topology', (req, res) => {
    const topology = {};
    for (const [mac, node] of Object.entries(nodes)) {
        topology[mac] = {
            neighbors: node.neighbors.map(n => n.mac),
            rssi: node.neighbors.map(n => n.rssi),
            state: node.state,
            color: node.color,
            temperature: node.temperature,
            mmwavePresence: node.mmwavePresence,
            mmwaveDistance: node.mmwaveDistance
        };
    }
    res.json(topology);
});

app.get('/api/stats', (req, res) => {
    res.json({
        nodeCount: Object.keys(nodes).length,
        messageCount,
        lastMessageTime,
        uptime: process.uptime()
    });
});

// MQTT status endpoint
app.get('/api/mqtt', (req, res) => {
    res.json({
        connected: mqttConnected,
        broker: MQTT_BROKER_URL,
        topologyTopic: MQTT_TOPOLOGY_TOPIC,
        stateTopic: MQTT_STATE_TOPIC,
        clientId: MQTT_CLIENT_ID
    });
});

// WebSocket server
const wss = new WebSocket.Server({ server });

// UDP socket
const udpSocket = dgram.createSocket('udp4');

// Start MQTT client
connectMQTT();

// ============================================================================
// UDP Message Handler
// ============================================================================

udpSocket.on('message', (buffer, remote) => {
    messageCount++;
    lastMessageTime = Date.now();
    
    // Parse the message
    const node = parseNodeUpdate(buffer);
    
    if (node === null) {
        console.warn(`Failed to parse message from ${remote.address}:${remote.port}`);
        return;
    }
    
    // Update node data
    nodes[node.mac] = node;
    
    // Broadcast to all WebSocket clients
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({ 
                type: 'node_update', 
                data: node 
            }));
        }
    });
    
    console.log(`Received update from ${node.mac} (RSSI: ${node.neighbors.length} neighbors, Temp: ${node.temperature}°C)`);
});

udpSocket.on('error', (err) => {
    console.error(`UDP socket error: ${err.message}`);
});

// ============================================================================
// WebSocket Connection Handler
// ============================================================================

wss.on('connection', (ws) => {
    console.log('New WebSocket client connected');
    
    // Send current state to new client
    ws.send(JSON.stringify({ 
        type: 'full_state', 
        data: Object.values(nodes) 
    }));
    
    ws.on('close', () => {
        console.log('WebSocket client disconnected');
    });
    
    ws.on('error', (err) => {
        console.error(`WebSocket error: ${err.message}`);
    });
});

// ============================================================================
// Server Startup
// ============================================================================

// Start UDP socket
udpSocket.bind(UDP_PORT, () => {
    console.log(`UDP socket listening on port ${UDP_PORT}`);
});

// Start HTTP server
server.listen(HTTP_PORT, () => {
    console.log(`HTTP server running on http://localhost:${HTTP_PORT}`);
    console.log(`WebSocket server running on ws://localhost:${WS_PORT}`);
    console.log('Mesh visualization server ready!');
});

// Handle process termination
process.on('SIGINT', () => {
    console.log('\nShutting down server...');
    disconnectMQTT();
    udpSocket.close();
    wss.close();
    server.close();
    process.exit(0);
});

// Periodic cleanup
setInterval(() => {
    const now = Date.now();
    const timeout = 10000; // 10 seconds
    
    for (const [mac, node] of Object.entries(nodes)) {
        if (now - node.receivedAt > timeout) {
            delete nodes[mac];
            console.log(`Removed stale node: ${mac}`);
        }
    }
    
    // Broadcast periodic update to clients
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({ 
                type: 'stats', 
                data: {
                    nodeCount: Object.keys(nodes).length,
                    messageCount,
                    lastMessageTime
                } 
            }));
        }
    });
}, 5000);

console.log('='.repeat(50));
console.log('  Mesh Visualization Server');
console.log('  Adaptive ESP32 Mesh Network');
console.log('  Supports: UDP (port ' + UDP_PORT + ') + MQTT (' + MQTT_BROKER_URL + ')');
console.log('='.repeat(50));
