#!/usr/bin/env node

/**
 * MQTT Monitor for Mesh Visualization
 * Monitors MQTT messages on mesh topics to help debug connectivity
 */

const mqtt = require('mqtt');

console.log('='.repeat(50));
console.log('  MQTT Monitor for Mesh Visualization');
console.log('='.repeat(50));
console.log('');

// Try to connect to localhost first
const brokerUrl = 'mqtt://localhost:1883';
console.log('Connecting to MQTT broker:', brokerUrl);
console.log('Subscribing to: mesh/#');
console.log('');

const client = mqtt.connect(brokerUrl, {
    clientId: 'mqtt_monitor',
    clean: true
});

client.on('connect', () => {
    console.log('✓ Connected to MQTT broker');
    
    // Subscribe to all mesh topics
    client.subscribe('mesh/#', { qos: 0 }, (err, granted) => {
        if (err) {
            console.error('✗ Subscription error:', err.message);
            process.exit(1);
        } else {
            console.log('✓ Subscribed to mesh/#');
            console.log('');
            console.log('Waiting for MQTT messages... (Press Ctrl+C to exit)');
            console.log('-'.repeat(50));
        }
    });
});

client.on('message', (topic, message) => {
    const timestamp = new Date().toISOString();
    const payload = message.toString();
    
    console.log(`[${timestamp}] ${topic}`);
    console.log('  Payload:', payload);
    console.log('');
    
    // Try to parse as JSON for better formatting
    try {
        const json = JSON.parse(payload);
        console.log('  Parsed JSON:');
        console.log('  ', JSON.stringify(json, null, 2));
    } catch (e) {
        // Not JSON, already shown as plain text
    }
    console.log('-'.repeat(50));
});

client.on('error', (err) => {
    console.error('✗ MQTT connection error:', err.message);
    console.log('');
    console.log('Troubleshooting:');
    console.log('1. Check if Mosquitto is running: sudo systemctl status mosquitto');
    console.log('2. Check listening ports: ss -tlnp | grep 1883');
    console.log('3. Try restarting Mosquitto: sudo systemctl restart mosquitto');
    process.exit(1);
});

client.on('close', () => {
    console.log('\n✗ Connection closed');
});

client.on('offline', () => {
    console.log('\n✗ MQTT broker offline');
});

// Handle Ctrl+C
process.on('SIGINT', () => {
    console.log('\nShutting down MQTT monitor...');
    client.end();
    process.exit(0);
});