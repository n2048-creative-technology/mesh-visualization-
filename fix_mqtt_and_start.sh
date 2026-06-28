#!/bin/bash

echo "=== Mesh Visualization MQTT Setup ==="
echo ""

# Check current machine IP
MY_IP=$(hostname -I | awk '{print $1}')
echo "Current machine IP: $MY_IP"
echo ""

# Check Mosquitto status
echo "Checking Mosquitto..."
if pgrep -x "mosquitto" > /dev/null; then
    echo "✓ Mosquitto is running"
    
    # Check if listening on all interfaces
    if ss -tlnp | grep -q "0.0.0.0:1883"; then
        echo "✓ Mosquitto is listening on all interfaces (0.0.0.0:1883)"
    else
        echo "✗ Mosquitto is NOT listening on all interfaces"
        echo "   Current listening ports:"
        ss -tlnp | grep 1883 | while read line; do echo "   $line"; done
        echo ""
        echo "To fix this, run:"
        echo "  echo 'listener 1883 0.0.0.0' | sudo tee /etc/mosquitto/conf.d/mesh.conf"
        echo "  sudo systemctl restart mosquitto"
        echo ""
        exit 1
    fi
else
    echo "✗ Mosquitto is NOT running"
    echo "Start it with: sudo systemctl start mosquitto"
    exit 1
fi

echo ""
echo "Starting visualization server..."
echo ""

# Change to visualization app directory
cd "$(dirname "$0")/visualization/app"

# Start the visualization server with the current machine IP
echo "Starting: MQTT_BROKER_URL=mqtt://$MY_IP:1883 node server.js"
MQTT_BROKER_URL=mqtt://$MY_IP:1883 node server.js