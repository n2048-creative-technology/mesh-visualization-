/**
 * Mesh Visualization Frontend
 * D3.js-based real-time visualization of adaptive ESP32 mesh network
 */

// ============================================================================
// Configuration
// ============================================================================

const WS_URL = `ws://${window.location.hostname}:${window.location.port}`;
const UPDATE_INTERVAL = 100; // ms

// Color schemes
const COLOR_SCHEMES = {
    rssi: {
        name: 'By RSSI Strength',
        node: (node) => {
            // Use average neighbor RSSI for node color
            if (node.neighbors.length === 0) return '#888888';
            const avgRssi = node.neighbors.reduce((sum, n) => sum + n.rssi, 0) / node.neighbors.length;
            return rssiToColor(avgRssi);
        },
        link: (rssi) => rssiToColor(rssi)
    },
    state: {
        name: 'By Node State',
        node: (node) => {
            switch (node.state) {
                case 0: return '#0000ff'; // Idle
                case 1: return '#00ff00'; // Active
                case 2: return '#ff0000'; // Error
                case 3: return '#ffff00'; // Booting
                default: return '#888888';
            }
        },
        link: () => '#666666'
    },
    temperature: {
        name: 'By Temperature',
        node: (node) => temperatureToColor(node.temperature),
        link: () => '#666666'
    },
    fixed: {
        name: 'Fixed Color',
        node: () => '#4a90e2',
        link: () => '#666666'
    }
};

// ============================================================================
// Global Variables
// ============================================================================

let nodes = {};
let links = [];
let svg, simulation, nodeElements, linkElements, labelElements, rssiElements;
let ws;
let isPaused = false;
let isConnected = false;
let mqttConnected = false;
let colorScheme = 'rssi';
let showLabels = true;
let showRssi = true;
let minRssi = -80;

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert RSSI to color (green = strong, red = weak)
 */
function rssiToColor(rssi) {
    // Clamp RSSI to range
    rssi = Math.max(-100, Math.min(0, rssi));
    
    // Normalize to 0-1 (0 = -100dBm, 1 = 0dBm)
    const normalized = (rssi + 100) / 100;
    
    // Green to red gradient
    const r = Math.floor(255 * (1 - normalized));
    const g = Math.floor(255 * normalized);
    const b = 0;
    
    return `rgb(${r}, ${g}, ${b})`;
}

/**
 * Convert temperature to color (blue = cold, red = hot)
 */
function temperatureToColor(temp) {
    // Assume temperature range of -20°C to 50°C
    const clamped = Math.max(-20, Math.min(50, temp));
    const normalized = (clamped + 20) / 70;
    
    // Blue to red gradient
    const r = Math.floor(255 * normalized);
    const g = 0;
    const b = Math.floor(255 * (1 - normalized));
    
    return `rgb(${r}, ${g}, ${b})`;
}

/**
 * Get node color based on current scheme
 */
function getNodeColor(node) {
    return COLOR_SCHEMES[colorScheme].node(node);
}

/**
 * Get link color based on current scheme
 */
function getLinkColor(rssi) {
    return COLOR_SCHEMES[colorScheme].link(rssi);
}

/**
 * Calculate link width based on RSSI
 */
function getLinkWidth(rssi) {
    // Scale from 1px to 4px based on RSSI
    const clamped = Math.max(-100, Math.min(0, rssi));
    const normalized = (clamped + 100) / 100;
    return 1 + 3 * normalized;
}

/**
 * Shorten MAC address for display
 */
function shortenMac(mac) {
    if (!mac) return '?';
    const parts = mac.split(':');
    return parts[parts.length - 1]; // Last byte
}

/**
 * Format RSSI for display
 */
function formatRssi(rssi) {
    return rssi === undefined ? '?' : `${rssi} dBm`;
}

/**
 * Format temperature for display
 */
function formatTemperature(temp) {
    return temp === undefined ? '?' : `${temp.toFixed(1)}°C`;
}

/**
 * Format distance for display
 */
function formatDistance(distance) {
    if (distance === undefined) return '?';
    if (distance < 1000) return `${distance} mm`;
    return `${(distance / 1000).toFixed(2)} m`;
}

// ============================================================================
// D3.js Setup
// ============================================================================

/**
 * Initialize D3.js visualization
 */
function initVisualization() {
    svg = d3.select('#visualization');
    
    // Create force simulation
    simulation = d3.forceSimulation()
        .force('link', d3.forceLink().id(d => d.mac).distance(150).strength(0.5))
        .force('charge', d3.forceManyBody().strength(-300).distanceMax(500))
        .force('center', d3.forceCenter(window.innerWidth / 2, window.innerHeight / 2 - 100))
        .force('collision', d3.forceCollide().radius(20))
        .alphaDecay(0.05);
    
    // Create container for links
    const linkContainer = svg.append('g').attr('class', 'links');
    
    // Create container for nodes
    const nodeContainer = svg.append('g').attr('class', 'nodes');
    
    // Create container for labels
    const labelContainer = svg.append('g').attr('class', 'labels');
    
    // Create container for RSSI labels
    const rssiContainer = svg.append('g').attr('class', 'rssi-labels');
    
    // Update functions
    function updateLinks() {
        linkElements = linkContainer.selectAll('.link')
            .data(links, d => `${d.source.mac}-${d.target.mac}`);
        
        // Enter new links
        linkElements.enter()
            .append('line')
            .attr('class', 'link')
            .attr('stroke-width', d => getLinkWidth(d.rssi))
            .attr('stroke', d => getLinkColor(d.rssi))
            .attr('opacity', 0.6)
            .merge(linkElements);
        
        // Update existing links
        linkElements
            .attr('stroke-width', d => getLinkWidth(d.rssi))
            .attr('stroke', d => getLinkColor(d.rssi));
        
        // Remove old links
        linkElements.exit().remove();
    }
    
    function updateNodes() {
        nodeElements = nodeContainer.selectAll('.node')
            .data(Object.values(nodes), d => d.mac);
        
        // Enter new nodes
        const newNodes = nodeElements.enter()
            .append('circle')
            .attr('class', 'node')
            .attr('r', 12)
            .attr('fill', d => getNodeColor(d))
            .attr('stroke', '#333')
            .attr('stroke-width', 2)
            .on('click', (event, d) => showNodeInfo(d))
            .on('mouseenter', function() {
                d3.select(this).attr('r', 16);
            })
            .on('mouseleave', function() {
                d3.select(this).attr('r', 12);
            });
        
        // Update existing nodes
        nodeElements
            .attr('fill', d => getNodeColor(d));
        
        // Remove old nodes
        nodeElements.exit().remove();
    }
    
    function updateLabels() {
        if (!showLabels) {
            if (labelElements) labelElements.remove();
            return;
        }
        
        labelElements = labelContainer.selectAll('.node-label')
            .data(Object.values(nodes), d => d.mac);
        
        labelElements.enter()
            .append('text')
            .attr('class', 'node-label')
            .attr('dy', -15)
            .text(d => shortenMac(d.mac))
            .merge(labelElements);
        
        labelElements.exit().remove();
    }
    
    function updateRssiLabels() {
        if (!showRssi) {
            if (rssiElements) rssiElements.remove();
            return;
        }
        
        // Only show RSSI for nodes with neighbors
        const nodesWithRssi = Object.values(nodes).filter(n => n.neighbors.length > 0);
        
        rssiElements = rssiContainer.selectAll('.node-rssi')
            .data(nodesWithRssi, d => d.mac);
        
        rssiElements.enter()
            .append('text')
            .attr('class', 'node-rssi')
            .attr('dy', 15)
            .text(d => {
                const avgRssi = d.neighbors.reduce((sum, n) => sum + n.rssi, 0) / d.neighbors.length;
                return formatRssi(Math.round(avgRssi));
            })
            .merge(rssiElements);
        
        rssiElements.exit().remove();
    }
    
    // Update simulation data
    function updateSimulation() {
        // Filter links based on min RSSI
        links = [];
        for (const [mac, node] of Object.entries(nodes)) {
            for (const neighbor of node.neighbors) {
                if (neighbor.rssi >= minRssi) {
                    links.push({
                        source: mac,
                        target: neighbor.mac,
                        rssi: neighbor.rssi
                    });
                }
            }
        }
        
        // Update simulation
        simulation
            .nodes(Object.values(nodes))
            .on('tick', ticked);
        
        simulation.force('link')
            .links(links);
        
        // Restart simulation
        simulation.alpha(1).restart();
    }
    
    // Tick function for simulation
    function ticked() {
        if (nodeElements) {
            nodeElements
                .attr('cx', d => d.x)
                .attr('cy', d => d.y);
        }
        
        if (linkElements) {
            linkElements
                .attr('x1', d => d.source.x)
                .attr('y1', d => d.source.y)
                .attr('x2', d => d.target.x)
                .attr('y2', d => d.target.y);
        }
        
        if (labelElements) {
            labelElements
                .attr('x', d => d.x)
                .attr('y', d => d.y);
        }
        
        if (rssiElements) {
            rssiElements
                .attr('x', d => d.x)
                .attr('y', d => d.y);
        }
    }
    
    // Initial update
    updateSimulation();
    updateLinks();
    updateNodes();
    updateLabels();
    updateRssiLabels();
    
    // Return update functions
    return {
        updateLinks,
        updateNodes,
        updateLabels,
        updateRssiLabels,
        updateSimulation,
        ticked
    };
}

// ============================================================================
// WebSocket Connection
// ============================================================================

/**
 * Connect to WebSocket server
 */
function connectWebSocket() {
    try {
        ws = new WebSocket(WS_URL);
        
        ws.onopen = () => {
            isConnected = true;
            updateConnectionStatus(true);
            console.log('WebSocket connected');
            
            // Check MQTT status via API
            checkMqttStatus();
        };
        
        ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            
            switch (data.type) {
                case 'full_state':
                    // Initial state
                    nodes = {};
                    for (const node of data.data) {
                        nodes[node.mac] = node;
                    }
                    updateVisualization();
                    updateStats();
                    break;
                    
                case 'node_update':
                    // Update single node
                    nodes[data.data.mac] = data.data;
                    updateVisualization();
                    updateStats();
                    break;
                    
                case 'stats':
                    // Update stats
                    updateStats();
                    break;
                    
                default:
                    console.log('Unknown message type:', data.type);
            }
        };
        
        ws.onclose = () => {
            isConnected = false;
            updateConnectionStatus(false);
            console.log('WebSocket disconnected');
            
            // Attempt to reconnect after delay
            setTimeout(connectWebSocket, 5000);
        };
        
        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            isConnected = false;
            updateConnectionStatus(false);
        };
        
    } catch (error) {
        console.error('Failed to create WebSocket:', error);
        isConnected = false;
        updateConnectionStatus(false);
    }
}

/**
 * Update WebSocket connection status indicator
 */
function updateConnectionStatus(connected) {
    const statusEl = document.querySelector('.connection-status');
    if (!statusEl) return;
    
    if (connected) {
        statusEl.classList.add('connected');
    } else {
        statusEl.classList.remove('connected');
    }
}

/**
 * Update MQTT connection status indicator
 */
function updateMqttConnectionStatus(connected) {
    mqttConnected = connected;
    const mqttStatusEl = document.querySelector('.mqtt-status');
    if (!mqttStatusEl) return;
    
    if (connected) {
        mqttStatusEl.classList.add('connected');
        mqttStatusEl.title = 'MQTT: Connected';
    } else {
        mqttStatusEl.classList.remove('connected');
        mqttStatusEl.title = 'MQTT: Disconnected';
    }
}

/**
 * Check MQTT status via API
 */
function checkMqttStatus() {
    if (!isConnected) return;
    
    try {
        fetch('/api/mqtt')
            .then(response => response.json())
            .then(data => {
                if (data.connected !== undefined) {
                    updateMqttConnectionStatus(data.connected);
                }
            })
            .catch(error => {
                console.error('Failed to check MQTT status:', error);
                updateMqttConnectionStatus(false);
            });
    } catch (error) {
        console.error('Failed to check MQTT status:', error);
        updateMqttConnectionStatus(false);
    }
}

// ============================================================================
// Visualization Update
// ============================================================================

let visualization;

/**
 * Update visualization with current data
 */
function updateVisualization() {
    if (!visualization || isPaused) return;
    
    visualization.updateSimulation();
    visualization.updateLinks();
    visualization.updateNodes();
    visualization.updateLabels();
    visualization.updateRssiLabels();
}

/**
 * Update statistics display
 */
function updateStats() {
    const nodeCount = Object.keys(nodes).length;
    const connectionCount = links.length;
    
    document.getElementById('node-count').textContent = `${nodeCount} Nodes`;
    document.getElementById('connection-count').textContent = `${connectionCount} Connections`;
    
    // Update last update time
    const now = new Date();
    document.getElementById('last-update').textContent = now.toLocaleTimeString();
}

// ============================================================================
// Node Information Panel
// ============================================================================

/**
 * Show node information panel
 */
function showNodeInfo(node) {
    const panel = document.getElementById('node-info-panel');
    const content = document.getElementById('node-info-content');
    
    // Create HTML for node info
    content.innerHTML = `
        <div class="node-info-row">
            <span class="node-info-label">MAC Address:</span>
            <span class="node-info-value">${node.mac}</span>
        </div>
        <div class="node-info-row">
            <span class="node-info-label">State:</span>
            <span class="node-info-value">${getStateName(node.state)}</span>
        </div>
        <div class="node-info-row">
            <span class="node-info-label">Temperature:</span>
            <span class="node-info-value">${formatTemperature(node.temperature)}</span>
        </div>
        <div class="node-info-row">
            <span class="node-info-label">mmWave Presence:</span>
            <span class="node-info-value">${node.mmwavePresence ? 'Yes' : 'No'}</span>
        </div>
        <div class="node-info-row">
            <span class="node-info-label">mmWave Distance:</span>
            <span class="node-info-value">${formatDistance(node.mmwaveDistance)}</span>
        </div>
        <div class="node-info-row">
            <span class="node-info-label">Neighbors:</span>
            <span class="node-info-value">${node.neighbors.length}</span>
        </div>
        <div class="node-info-row">
            <span class="node-info-label">Avg RSSI:</span>
            <span class="node-info-value">${node.neighbors.length > 0 ? 
                formatRssi(Math.round(node.neighbors.reduce((sum, n) => sum + n.rssi, 0) / node.neighbors.length)) : 'N/A'}</span>
        </div>
        <div class="node-info-row">
            <span class="node-info-label">Last Update:</span>
            <span class="node-info-value">${new Date(node.receivedAt).toLocaleTimeString()}</span>
        </div>
    `;
    
    // Add neighbor details
    if (node.neighbors.length > 0) {
        content.innerHTML += `<div class="node-info-row" style="margin-top: 10px;">
            <span class="node-info-label">Neighbor Details:</span>
        </div>`;
        
        for (const neighbor of node.neighbors) {
            content.innerHTML += `
                <div class="node-info-row" style="padding-left: 20px;">
                    <span class="node-info-label">${shortenMac(neighbor.mac)}:</span>
                    <span class="node-info-value">${formatRssi(neighbor.rssi)}</span>
                </div>
            `;
        }
    }
    
    panel.classList.add('active');
}

/**
 * Hide node information panel
 */
function hideNodeInfo() {
    document.getElementById('node-info-panel').classList.remove('active');
}

/**
 * Get state name from state code
 */
function getStateName(state) {
    switch (state) {
        case 0: return 'Idle';
        case 1: return 'Active';
        case 2: return 'Error';
        case 3: return 'Booting';
        default: return 'Unknown';
    }
}

// ============================================================================
// Event Handlers
// ============================================================================

/**
 * Handle color scheme change
 */
function onColorSchemeChange() {
    const select = document.getElementById('color-scheme');
    colorScheme = select.value;
    updateVisualization();
}

/**
 * Handle show labels toggle
 */
function onShowLabelsChange() {
    showLabels = document.getElementById('show-labels').checked;
    updateVisualization();
}

/**
 * Handle show RSSI toggle
 */
function onShowRssiChange() {
    showRssi = document.getElementById('show-rssi').checked;
    updateVisualization();
}

/**
 * Handle min RSSI slider change
 */
function onMinRssiChange() {
    const slider = document.getElementById('min-rssi');
    minRssi = parseInt(slider.value);
    document.getElementById('min-rssi-value').textContent = minRssi;
    updateVisualization();
}

/**
 * Reset view
 */
function resetView() {
    if (simulation) {
        simulation
            .force('center', d3.forceCenter(window.innerWidth / 2, window.innerHeight / 2 - 100))
            .alpha(1)
            .restart();
    }
}

/**
 * Toggle pause
 */
function togglePause() {
    isPaused = !isPaused;
    const button = document.getElementById('pause-simulation');
    button.textContent = isPaused ? 'Resume' : 'Pause';
    
    if (!isPaused) {
        updateVisualization();
    }
}

// ============================================================================
// Window Events
// ============================================================================

/**
 * Handle window resize
 */
function onWindowResize() {
    svg.attr('width', window.innerWidth)
       .attr('height', window.innerHeight - 200);
    
    if (simulation) {
        simulation.force('center', d3.forceCenter(window.innerWidth / 2, window.innerHeight / 2 - 100));
    }
    
    updateVisualization();
}

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize the application
 */
function init() {
    // Initialize visualization
    visualization = initVisualization();
    
    // Connect to WebSocket
    connectWebSocket();
    
    // Set up event listeners
    document.getElementById('color-scheme').addEventListener('change', onColorSchemeChange);
    document.getElementById('show-labels').addEventListener('change', onShowLabelsChange);
    document.getElementById('show-rssi').addEventListener('change', onShowRssiChange);
    document.getElementById('min-rssi').addEventListener('input', onMinRssiChange);
    document.getElementById('reset-view').addEventListener('click', resetView);
    document.getElementById('pause-simulation').addEventListener('click', togglePause);
    document.getElementById('close-node-info').addEventListener('click', hideNodeInfo);
    
    // Handle window resize
    window.addEventListener('resize', onWindowResize);
    
    // Initial resize
    onWindowResize();
    
    // Create connection status indicators
    const wsStatusEl = document.createElement('div');
    wsStatusEl.className = 'connection-status';
    wsStatusEl.title = 'WebSocket: Disconnected';
    document.body.appendChild(wsStatusEl);
    
    const mqttStatusEl = document.createElement('div');
    mqttStatusEl.className = 'mqtt-status';
    mqttStatusEl.title = 'MQTT: Disconnected';
    document.body.appendChild(mqttStatusEl);
    
    // Check MQTT status periodically
    setInterval(checkMqttStatus, 10000); // Check every 10 seconds
    
    console.log('Mesh visualization initialized');
}

// Start the application when DOM is loaded
document.addEventListener('DOMContentLoaded', init);
