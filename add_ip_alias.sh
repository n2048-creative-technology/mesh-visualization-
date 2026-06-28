#!/bin/bash

echo "=== Adding IP Alias for MQTT Broker ==="
echo ""
echo "Current network interfaces:"
ip addr show | grep -E "^[0-9]+:|inet "
echo ""

# Find the active network interface (not loopback)
INTERFACE=$(ip route | grep default | awk '{print $5}')
if [ -z "$INTERFACE" ]; then
    # Try to find first non-loopback interface with an IP
    INTERFACE=$(ip addr show | grep -E "^[0-9]+: " | grep -v "1: lo" | head -1 | awk -F: '{print $2}' | tr -d ' ')
fi

echo "Using network interface: $INTERFACE"

# Add the firmware's expected IP as an alias
sudo ip addr add 10.64.5.196/20 dev $INTERFACE

# Verify
echo ""
echo "Updated network interfaces:"
ip addr show $INTERFACE
echo ""

# Test if we can ping the new IP
echo "Testing connectivity to 10.64.5.196..."
if ping -c 1 -W 1 10.64.5.196 &> /dev/null; then
    echo "✓ Successfully added 10.64.5.196 to $INTERFACE"
else
    echo "✗ Failed to add IP alias"
fi
echo ""
echo "Now start the visualization app with:"
echo "  cd visualization/app"
echo "  MQTT_BROKER_URL=mqtt://10.64.5.196:1883 node server.js"