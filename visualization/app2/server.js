/**
 * Mesh Visualization Server - Minimalistic Version
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

const UDP_PORT = 1236;
const HTTP_PORT = 3003;
const WS_PORT = 3003;

// ============================================================================
// MQTT Configuration
// ============================================================================

const MQTT_BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883';
const MQTT_TOPOLOGY_TOPIC = process.env.MQTT_TOPOLOGY_TOPIC || 'mesh/topology';
const MQTT_STATE_TOPIC = process.env.MQTT_STATE_TOPIC || 'mesh/state';
const MQTT_COMMAND_TOPIC = process.env.MQTT_COMMAND_TOPIC || 'mesh/commands';
const MQTT_CLIENT_ID = process.env.MQTT_CLIENT_ID || 'mesh_visualization_server_minimal';

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

const nodes = {};
const STALE_NODE_TIMEOUT_MS = Number(process.env.STALE_NODE_TIMEOUT_MS || 30000);
let latestTopology = {
    rootMac: null,
    routingTableSize: 0,
    routeSampleCount: 0,
    routingTableTruncated: false,
    updatedAt: null
};

let messageCount = 0;
let lastMessageTime = Date.now();

// ============================================================================
// UDP Message Parser
// ============================================================================

const PROTOCOL_VERSION = 0x01;
const MSG_TYPE_STATE_UPDATE = 0x01;
const MSG_TYPE_BEACON = 0x02;
const MSG_TYPE_FORWARD = 0x03;

function parseNodeUpdate(buffer) {
    if (buffer.length !== 77) {
        return null;
    }

    let offset = 0;

    const version = buffer.readUInt8(offset);
    offset += 1;

    if (version !== PROTOCOL_VERSION) {
        console.warn(`Unsupported protocol version: ${version}`);
        return null;
    }

    const msgType = buffer.readUInt8(offset);
    offset += 1;

    const mac = buffer.slice(offset, offset + 6).toString('hex').match(/.{1,2}/g).join(':');
    offset += 6;

    const state = buffer.readUInt8(offset);
    offset += 1;

    const color = {
        r: buffer.readUInt8(offset),
        g: buffer.readUInt8(offset + 1),
        b: buffer.readUInt8(offset + 2)
    };
    offset += 3;

    const temperature = buffer.readInt16LE(offset) / 10.0;
    offset += 2;

    const mmwavePresence = buffer.readUInt8(offset);
    offset += 1;

    const mmwaveDistance = buffer.readUInt32LE(offset);
    offset += 4;

    const timestamp = buffer.readUInt32LE(offset);
    offset += 4;

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

    const receivedChecksum = buffer.readUInt16LE(offset);
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

function convertMqttTopologyToNode(topologyMsg) {
    const node = {
        version: PROTOCOL_VERSION,
        msgType: MSG_TYPE_STATE_UPDATE,
        mac: topologyMsg.node_mac,
        state: topologyMsg.node_state || 1,
        value: topologyMsg.value || 0,
        color: { r: 0, g: 255, b: 0 },
        temperature: 0,
        mmwavePresence: false,
        mmwaveDistance: 0,
        timestamp: Date.now() / 1000,
        neighbors: [],
        receivedAt: Date.now()
    };

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

function createPlaceholderNode(mac) {
    return {
        version: PROTOCOL_VERSION,
        msgType: MSG_TYPE_STATE_UPDATE,
        mac,
        state: 1,
        value: 0,
        color: { r: 128, g: 128, b: 128 },
        temperature: 0,
        mmwavePresence: false,
        mmwaveDistance: 0,
        timestamp: Math.floor(Date.now() / 1000),
        neighbors: [],
        receivedAt: Date.now(),
        discoveredFromTopology: true
    };
}

function ensureTopologyNode(mac) {
    if (!mac || typeof mac !== 'string') return null;
    if (!nodes[mac]) {
        nodes[mac] = createPlaceholderNode(mac);
    }
    nodes[mac].receivedAt = Date.now();
    return nodes[mac];
}

function ensureTopologyReferences(topologyMsg, sourceMac) {
    const referenced = new Set();
    const referencedNodes = [];

    if (Array.isArray(topologyMsg.neighbors)) {
        for (const neighbor of topologyMsg.neighbors) {
            if (neighbor && neighbor.mac) referenced.add(neighbor.mac);
        }
    }

    const routeSample = topologyMsg.routing_table_sample || topologyMsg.routing_table || [];
    if (Array.isArray(routeSample)) {
        for (const mac of routeSample) {
            if (mac) referenced.add(mac);
        }
    }

    referenced.delete(sourceMac);

    for (const mac of referenced) {
        const referencedNode = ensureTopologyNode(mac);
        if (referencedNode) referencedNodes.push(referencedNode);
    }

    return referencedNodes;
}

function convertMqttStateToNode(stateMsg) {
    const node = {
        version: PROTOCOL_VERSION,
        msgType: MSG_TYPE_STATE_UPDATE,
        mac: stateMsg.mac,
        state: stateMsg.state || 1,
        value: stateMsg.value || 0,
        color: {
            r: stateMsg.color && Array.isArray(stateMsg.color) && stateMsg.color[0] !== undefined ? stateMsg.color[0] : 0,
            g: stateMsg.color && Array.isArray(stateMsg.color) && stateMsg.color[1] !== undefined ? stateMsg.color[1] : 255,
            b: stateMsg.color && Array.isArray(stateMsg.color) && stateMsg.color[2] !== undefined ? stateMsg.color[2] : 0
        },
        temperature: (stateMsg.temperature !== undefined ? stateMsg.temperature : 0) / 10.0,
        mmwavePresence: (stateMsg.mmwave_presence || 0) !== 0,
        mmwaveDistance: stateMsg.mmwave_distance || 0,
        timestamp: stateMsg.timestamp || Math.floor(Date.now() / 1000),
        kernel_sequence: stateMsg.kernel_sequence,
        value_sequence: stateMsg.value_sequence,
        activation_sequence: stateMsg.activation_sequence,
        activation_count: stateMsg.activation_count,
        kernel_function: stateMsg.kernel_function,
        activation_function: stateMsg.activation_function,
        kernel: stateMsg.kernel,
        activations: stateMsg.activations,
        neighbors: [],
        receivedAt: Date.now()
    };

    if (Array.isArray(stateMsg.neighbors)) {
        for (const neighbor of stateMsg.neighbors) {
            if (neighbor && neighbor.mac && neighbor.rssi !== undefined) {
                node.neighbors.push({
                    mac: neighbor.mac,
                    rssi: neighbor.rssi
                });
            }
        }
    }

    return node;
}

// ============================================================================
// MQTT Client Setup
// ============================================================================

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

function handleMqttMessage(topic, payload) {
    messageCount++;
    lastMessageTime = Date.now();

    let node = null;

    switch (topic) {
        case MQTT_TOPOLOGY_TOPIC:
            node = convertMqttTopologyToNode(payload);
            break;
        case MQTT_STATE_TOPIC:
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

    const existingNode = nodes[node.mac];

    if (existingNode) {
        if (topic === MQTT_TOPOLOGY_TOPIC) {
            existingNode.neighbors = node.neighbors;
            existingNode.state = node.state;
            existingNode.value = node.value;
        } else if (topic === MQTT_STATE_TOPIC) {
            existingNode.state = node.state;
            existingNode.value = node.value;
            existingNode.color = node.color;
            existingNode.temperature = node.temperature;
            existingNode.mmwavePresence = node.mmwavePresence;
            existingNode.mmwaveDistance = node.mmwaveDistance;
            existingNode.timestamp = node.timestamp;
            existingNode.kernel_sequence = node.kernel_sequence;
            existingNode.value_sequence = node.value_sequence;
            existingNode.activation_sequence = node.activation_sequence;
            existingNode.activation_count = node.activation_count;
            existingNode.kernel_function = node.kernel_function;
            existingNode.activation_function = node.activation_function;
            existingNode.kernel = node.kernel;
            existingNode.activations = node.activations;
            if (node.neighbors.length > 0) {
                existingNode.neighbors = node.neighbors;
            }
            existingNode.discoveredFromTopology = false;
        }
        existingNode.receivedAt = Date.now();
        node = existingNode;
    } else {
        nodes[node.mac] = node;
    }

    const additionalUpdates = [];
    if (topic === MQTT_TOPOLOGY_TOPIC) {
        latestTopology = {
            rootMac: payload.node_mac || node.mac,
            routingTableSize: Number(payload.routing_table_size || 0),
            routeSampleCount: Number(payload.route_sample_count || 0),
            routingTableTruncated: Boolean(payload.routing_table_truncated),
            updatedAt: Date.now()
        };
        additionalUpdates.push(...ensureTopologyReferences(payload, node.mac));
    }

    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({ 
                type: 'node_update', 
                data: node 
            }));
            for (const update of additionalUpdates) {
                client.send(JSON.stringify({
                    type: 'node_update',
                    data: update
                }));
            }
        }
    });

    console.log(`MQTT: Received ${topic} from ${node.mac} (Neighbors: ${node.neighbors.length}, Temp: ${node.temperature}°C)`);
}

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

function normalizeMac(mac) {
    if (typeof mac !== 'string') return null;
    const normalized = mac.trim().toLowerCase();
    return /^([0-9a-f]{2}:){5}[0-9a-f]{2}$/.test(normalized) ? normalized : null;
}

// ============================================================================
// Server Setup
// ============================================================================

const app = express();
const server = require('http').createServer(app);

app.use(express.json());
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

app.get('/api/mqtt', (req, res) => {
    res.json({
        connected: mqttConnected,
        broker: MQTT_BROKER_URL
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

    if (buffer.length !== 77) {
        return;
    }

    const node = parseNodeUpdate(buffer);

    if (node === null) {
        console.warn(`Failed to parse message from ${remote.address}:${remote.port}`);
        return;
    }

    nodes[node.mac] = node;

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

udpSocket.bind(UDP_PORT, () => {
    console.log(`UDP socket listening on port ${UDP_PORT}`);
});

server.listen(HTTP_PORT, () => {
    console.log(`HTTP server running on http://localhost:${HTTP_PORT}`);
    console.log(`WebSocket server running on ws://localhost:${WS_PORT}`);
    console.log('Minimalistic mesh visualization server ready!');
});

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
    const timeout = STALE_NODE_TIMEOUT_MS;

    for (const [mac, node] of Object.entries(nodes)) {
        if (now - node.receivedAt > timeout) {
            delete nodes[mac];
            console.log(`Removed stale node: ${mac}`);
        }
    }

    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({ 
                type: 'stats', 
                data: {
                    nodeCount: Object.keys(nodes).length,
                    meshReportedNodeCount: latestTopology.routingTableSize,
                    latestTopology,
                    messageCount,
                    lastMessageTime
                } 
            }));
        }
    });
}, 5000);

console.log('='.repeat(50));
console.log('  Cyberpunk Mesh Visualization Server');
console.log('  Port:', HTTP_PORT);
console.log('  Dark immersive background with blue/green glitch patterns');
console.log('='.repeat(50));
