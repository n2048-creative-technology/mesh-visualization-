#!/bin/bash

echo "Starting Simple MQTT Broker for Mesh Visualization..."
echo "This broker will listen on all network interfaces (0.0.0.0:1883)"
echo "Press Ctrl+C to stop"
echo ""

cd "$(dirname "$0")/visualization/app"

node -e "
const aedes = require('aedes');
const server = require('net').createServer(aedes.handle);
const port = 1883;

server.listen(port, '0.0.0.0', function() {
    console.log('✓ MQTT broker running on 0.0.0.0:' + port);
    console.log('✓ Listening on all network interfaces');
    console.log('✓ Allowing anonymous connections');
    console.log('');
    console.log('Firmware should connect to: ' + require('os').networkInterfaces().eth0 ? require('os').networkInterfaces().eth0[0].address : '10.65.5.196');
});

aedes.on('client', function(client) {
    console.log('✓ New client connected:', client.id);
});

aedes.on('publish', function(packet, client) {
    if (packet.topic.includes('mesh')) {
        console.log('✓ Message published to', packet.topic, 'by', client ? client.id : 'unknown');
    }
});

aedes.on('subscribe', function(subscriptions, client) {
    console.log('✓ Client', client.id, 'subscribed to:', subscriptions.map(s => s.topic).join(', '));
});

process.on('SIGINT', function() {
    console.log('\\nShutting down MQTT broker...');
    server.close();
    process.exit(0);
});
"