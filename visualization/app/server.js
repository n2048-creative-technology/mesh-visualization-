/**
 * Mesh Visualization Server
 * Node.js backend for UDP listener and WebSocket bridge
 * Receives binary UDP messages from ESP32 nodes and broadcasts to web clients
 */

const dgram = require('dgram');
const WebSocket = require('ws');
const express = require('express');
const path = require('path');

// ============================================================================
// Configuration
// ============================================================================

const UDP_PORT = 1234;
const HTTP_PORT = 3000;
const WS_PORT = 3000;

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

// WebSocket server
const wss = new WebSocket.Server({ server });

// UDP socket
const udpSocket = dgram.createSocket('udp4');

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
console.log('='.repeat(50));
