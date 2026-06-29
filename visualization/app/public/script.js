/**
 * Mesh Visualization Frontend
 * D3.js-based real-time visualization of adaptive ESP32 mesh network
 */

// ============================================================================
// Configuration
// ============================================================================

const WS_URL = `ws://${window.location.hostname}:${window.location.port}`;
const UPDATE_INTERVAL = 100; // ms

// ============================================================================
// Global Variables
// ============================================================================

let nodes = {};
let links = [];
let svg, viewport, zoomBehavior, simulation, nodeElements, linkElements, labelElements, rssiElements, detailElements;
let ws;
let isPaused = false;
let isConnected = false;
let mqttConnected = false;
let latestServerStats = null;
let activeTab = 'topology';
let showLabels = true;
let showRssi = true;
let minRssi = -80;
let selectedNodeMac = null;
let highlightedNodeMac = null;
let nodeCommandStatus = '';
let configStatus = '';

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
 * Get node color from LED/value status.
 */
function getNodeColor(node) {
    if (node.value === 1) return '#00ff00';
    if (node.value === 0) return '#ff0000';
    return '#888888';
}

/**
 * Get link color from RSSI so signal quality remains readable.
 */
function getLinkColor(rssi) {
    return rssiToColor(rssi);
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

function getLinkDistance(rssi) {
    const clamped = Math.max(-95, Math.min(-25, rssi));
    const normalized = (clamped + 95) / 70;
    return 260 - normalized * 170;
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

function compactDetail(value, maxLength = 28) {
    const text = value === undefined || value === null || value === '' ? '?' : String(value);
    return text.length > maxLength ? `${text.slice(0, maxLength - 3)}...` : text;
}

function formatNodeDetails(node) {
    const neighbors = Array.isArray(node.neighbors) ? node.neighbors : [];
    const avgRssi = neighbors.length > 0
        ? Math.round(neighbors.reduce((sum, n) => sum + n.rssi, 0) / neighbors.length)
        : null;
    const status = node.value === 1 ? 'ON' : node.value === 0 ? 'OFF' : 'WAIT';
    const kernelSeq = node.kernel_sequence ?? node.kernelSequence ?? '?';
    const activationSeq = node.activation_sequence ?? node.activationSequence ?? '?';
    const valueSeq = node.value_sequence ?? node.valueSequence ?? '?';
    const activationCount = node.activation_count ?? node.activationCount ?? '?';
    const presence = node.mmwavePresence ? 'yes' : 'no';

    return [
        `${shortenMac(node.mac)}  ${status}`,
        `state ${node.state ?? '?'} ${getStateName(node.state)}`,
        `value ${node.value ?? '?'}  seq ${valueSeq}`,
        `temp ${formatTemperature(node.temperature)}`,
        `presence ${presence}  ${formatDistance(node.mmwaveDistance)}`,
        `neighbors ${neighbors.length}  rssi ${avgRssi === null ? 'N/A' : `${avgRssi} dBm`}`,
        `kernel ${kernelSeq} ${compactDetail(node.kernel_function)}`,
        `activation ${activationSeq}/${activationCount} ${compactDetail(node.activation_function)}`
    ];
}

function sourceMac(link) {
    return typeof link.source === 'string' ? link.source : link.source.mac;
}

function targetMac(link) {
    return typeof link.target === 'string' ? link.target : link.target.mac;
}

function mergeNodeData(current, incoming) {
    if (!current) return incoming;

    const physics = {
        x: current.x,
        y: current.y,
        vx: current.vx,
        vy: current.vy,
        fx: current.fx,
        fy: current.fy
    };

    return Object.assign(current, incoming, physics);
}

// ============================================================================
// D3.js Setup
// ============================================================================

/**
 * Initialize D3.js visualization
 */
function initVisualization() {
    svg = d3.select('#visualization');
    viewport = svg.append('g').attr('class', 'viewport');
    zoomBehavior = d3.zoom()
        .scaleExtent([0.2, 5])
        .filter(event => {
            if (event.type === 'dblclick') return false;
            return !event.target.closest || !event.target.closest('.node');
        })
        .on('zoom', event => {
            viewport.attr('transform', event.transform);
        });

    svg.call(zoomBehavior);
    svg.on('dblclick.zoom', null);
    
    // Create force simulation
    simulation = d3.forceSimulation()
        .force('link', d3.forceLink().id(d => d.mac).distance(d => getLinkDistance(d.rssi)).strength(0.55))
        .force('charge', d3.forceManyBody().strength(-260).distanceMax(500))
        .force('center', d3.forceCenter(window.innerWidth / 2, window.innerHeight / 2 - 100))
        .force('collision', d3.forceCollide().radius(20))
        .velocityDecay(0.45)
        .alphaDecay(0.018);
    
    // Create container for links
    const linkContainer = viewport.append('g').attr('class', 'links');

    // Create container for floating node detail cards behind clickable nodes
    const detailContainer = viewport.append('g').attr('class', 'node-details');

    // Create container for nodes
    const nodeContainer = viewport.append('g').attr('class', 'nodes');
    
    // Create container for labels
    const labelContainer = viewport.append('g').attr('class', 'labels');
    
    // Create container for RSSI labels
    const rssiContainer = viewport.append('g').attr('class', 'rssi-labels');
    
    // Update functions
    function updateLinks() {
        linkElements = linkContainer.selectAll('.link')
            .data(links, d => `${sourceMac(d)}-${targetMac(d)}`);
        
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
        const nodeSelection = nodeContainer.selectAll('.node')
            .data(Object.values(nodes), d => d.mac);
        
        // Enter new nodes
        const newNodes = nodeSelection.enter()
            .append('circle')
            .attr('class', 'node')
            .on('click', (event, d) => {
                event.stopPropagation();
                selectedNodeMac = d.mac;
                nodeCommandStatus = '';
                toggleNodeStatus(d);
                updateVisualization();
            })
            .on('mouseenter', function(event, d) {
                startNodeHighlight(d);
            })
            .on('mouseleave', function(event, d) {
                stopNodeHighlight(d);
            });
        
        // Update nodes
        nodeElements = newNodes.merge(nodeSelection)
            .attr('fill', d => getNodeColor(d))
            .attr('r', d => d.mac === selectedNodeMac ? 14 : 12)
            .attr('stroke', d => d.mac === selectedNodeMac ? '#ffffff' : '#333')
            .attr('stroke-width', d => d.mac === selectedNodeMac ? 4 : 2);
        
        // Remove old nodes
        nodeSelection.exit().remove();
    }

    function updateNodeDetails() {
        if (!showLabels) {
            if (detailElements) detailElements.remove();
            return;
        }

        const nodeData = Object.values(nodes);
        const detailSelection = detailContainer.selectAll('.node-detail')
            .data(nodeData, d => d.mac);

        const newDetails = detailSelection.enter()
            .append('g')
            .attr('class', 'node-detail');

        newDetails.append('rect')
            .attr('class', 'node-detail-bg')
            .attr('rx', 4)
            .attr('ry', 4);

        detailElements = newDetails.merge(detailSelection);
        detailElements.each(function(d) {
            const group = d3.select(this);
            const lines = formatNodeDetails(d);
            const textSelection = group.selectAll('text')
                .data(lines);

            textSelection.enter()
                .append('text')
                .attr('class', 'node-detail-text')
                .merge(textSelection)
                .attr('x', 8)
                .attr('y', (_, i) => 14 + i * 12)
                .text(line => line);

            textSelection.exit().remove();

            const width = Math.max(...lines.map(line => line.length)) * 6 + 16;
            const height = lines.length * 12 + 8;
            group.select('.node-detail-bg')
                .attr('width', width)
                .attr('height', height)
                .attr('stroke', d.mac === selectedNodeMac ? '#ffffff' : '#384466');
        });

        detailSelection.exit().remove();
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
            .distance(d => getLinkDistance(d.rssi))
            .links(links);
        
        // Restart simulation
        simulation.alphaTarget(0.08).restart();
        window.clearTimeout(updateSimulation.alphaTimer);
        updateSimulation.alphaTimer = window.setTimeout(() => {
            simulation.alphaTarget(0);
        }, 1400);
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

        if (detailElements) {
            detailElements
                .attr('transform', d => `translate(${d.x + 18}, ${d.y - 36})`);
        }
    }
    
    // Initial update
    updateSimulation();
    updateLinks();
    updateNodes();
    updateLabels();
    updateRssiLabels();
    updateNodeDetails();
    
    // Return update functions
    return {
        updateLinks,
        updateNodes,
        updateLabels,
        updateRssiLabels,
        updateNodeDetails,
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
                    for (const node of data.data) {
                        nodes[node.mac] = mergeNodeData(nodes[node.mac], node);
                    }
                    updateVisualization();
                    updateStats();
                    break;
                    
                case 'node_update':
                    // Update single node
                    nodes[data.data.mac] = mergeNodeData(nodes[data.data.mac], data.data);
                    updateVisualization();
                    if (data.data.mac === selectedNodeMac) {
                        nodeCommandStatus = '';
                    }
                    updateStats();
                    break;

                case 'command_result':
                    if (!data.data.ok) {
                        console.warn('Command failed:', data.data.error);
                        nodeCommandStatus = data.data.error || 'Command failed';
                        configStatus = nodeCommandStatus;
                        updateConfigStatus();
                    } else if (data.data.target_mac === selectedNodeMac) {
                        nodeCommandStatus = 'Toggle sent';
                    } else if (data.data.command === 'config_update') {
                        configStatus = 'Sent to MQTT';
                        updateConfigStatus();
                    }
                    break;
                    
                case 'stats':
                    // Update stats
                    latestServerStats = data.data;
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
    visualization.updateNodeDetails();
}

function toggleNodeStatus(node) {
    if (!node || !node.mac || node.discoveredFromTopology) return false;

    selectedNodeMac = node.mac;
    nodeCommandStatus = 'Sending toggle...';
    updateVisualization();

    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            type: 'toggle_node',
            mac: node.mac
        }));
        return true;
    }

    fetch(`/api/nodes/${encodeURIComponent(node.mac)}/toggle`, { method: 'POST' })
        .then(response => response.json())
        .then(result => {
            nodeCommandStatus = result.ok ? 'Toggle sent' : (result.error || 'Command failed');
            updateVisualization();
        })
        .catch(error => {
            nodeCommandStatus = 'Command failed';
            console.error('Failed to toggle node:', error);
            updateVisualization();
        });

    return true;
}

function startNodeHighlight(node) {
    if (!node || !node.mac || node.discoveredFromTopology) return false;

    if (highlightedNodeMac && highlightedNodeMac !== node.mac) {
        const previousNode = nodes[highlightedNodeMac];
        if (previousNode) {
            previousNode.fx = null;
            previousNode.fy = null;
        }
        sendNodeHighlight({ mac: highlightedNodeMac }, false);
    }

    if (Number.isFinite(node.x) && Number.isFinite(node.y)) {
        node.fx = node.x;
        node.fy = node.y;
    }

    return sendNodeHighlight(node, true);
}

function stopNodeHighlight(node) {
    if (!node || !node.mac) return false;

    node.fx = null;
    node.fy = null;
    return sendNodeHighlight(node, false);
}

function sendNodeHighlight(node, enabled) {
    if (!node || !node.mac || node.discoveredFromTopology) return false;
    if (enabled && highlightedNodeMac === node.mac) return true;
    if (!enabled && highlightedNodeMac !== node.mac) return true;

    highlightedNodeMac = enabled ? node.mac : null;

    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            type: 'highlight_node',
            mac: node.mac,
            enabled
        }));
        return true;
    }

    fetch(`/api/nodes/${encodeURIComponent(node.mac)}/highlight`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled })
    }).catch(error => {
        console.error('Failed to highlight node:', error);
    });

    return true;
}

window.addEventListener('beforeunload', () => {
    if (highlightedNodeMac) {
        sendNodeHighlight({ mac: highlightedNodeMac }, false);
    }
});

/**
 * Update statistics display
 */
function updateStats() {
    const nodeCount = Object.keys(nodes).length;
    const meshReportedNodeCount = latestServerStats?.meshReportedNodeCount || 0;
    const connectionCount = links.length;

    document.getElementById('node-count').textContent =
        meshReportedNodeCount > 0 && meshReportedNodeCount !== nodeCount
            ? `${nodeCount} Nodes (${meshReportedNodeCount} mesh)`
            : `${nodeCount} Nodes`;
    document.getElementById('connection-count').textContent = `${connectionCount} Connections`;
    
    // Update last update time
    const now = new Date();
    document.getElementById('last-update').textContent = now.toLocaleTimeString();
}

/**
 * Get state name from state code
 */
function getStateName(state) {
    switch (state) {
        case 0: return 'Booting';
        case 1: return 'Idle';
        case 2: return 'Active';
        case 3: return 'Error';
        default: return 'Unknown';
    }
}

// ============================================================================
// Kernel / Activation Configuration
// ============================================================================

function initConfigEditor() {
    const kernelGrid = document.getElementById('kernel-grid');
    kernelGrid.innerHTML = '';

    for (let i = 0; i < 9; i++) {
        const field = document.createElement('label');
        field.className = 'kernel-field';
        field.innerHTML = `<span>k${i}</span><input id="kernel-${i}" type="number" step="0.1" value="0">`;
        kernelGrid.appendChild(field);
    }

    setKernelValues([1, 1, 1, 1, 1, 1, 1, 1, 0]);
    clearActivations();
    addActivation(3, 4.5);
}

function getKernelValues() {
    const values = [];
    for (let i = 0; i < 9; i++) {
        const value = Number(document.getElementById(`kernel-${i}`).value);
        if (!Number.isFinite(value)) {
            throw new Error(`Invalid k${i}`);
        }
        values.push(value);
    }
    return values;
}

function setKernelValues(values) {
    values.forEach((value, i) => {
        const input = document.getElementById(`kernel-${i}`);
        if (input) input.value = value;
    });
}

function addActivation(op = 2, value = 0) {
    const list = document.getElementById('activation-list');
    const row = document.createElement('div');
    row.className = 'activation-row';
    row.innerHTML = `
        <select class="activation-op">
            <option value="0">&lt;</option>
            <option value="1">&lt;=</option>
            <option value="2">==</option>
            <option value="3">&gt;=</option>
            <option value="4">&gt;</option>
        </select>
        <input class="activation-value" type="number" step="0.1" value="${value}">
    `;
    row.querySelector('.activation-op').value = String(op);
    list.appendChild(row);
}

function clearActivations() {
    document.getElementById('activation-list').innerHTML = '';
}

function getActivations() {
    return Array.from(document.querySelectorAll('.activation-row')).map((row) => {
        const op = Number(row.querySelector('.activation-op').value);
        const value = Number(row.querySelector('.activation-value').value);
        if (!Number.isFinite(op) || !Number.isFinite(value)) {
            throw new Error('Invalid activation');
        }
        return { op, value };
    });
}

function updateConfigStatus() {
    const statusEl = document.getElementById('config-status');
    if (statusEl) statusEl.textContent = configStatus;
}

function sendConfigCommand(payload) {
    configStatus = 'Sending...';
    updateConfigStatus();

    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            type: 'config_update',
            data: payload
        }));
        return;
    }

    fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
        .then(response => response.json())
        .then(result => {
            configStatus = result.ok ? 'Sent to MQTT' : (result.error || 'Command failed');
            updateConfigStatus();
        })
        .catch(error => {
            configStatus = 'Command failed';
            console.error('Failed to send config:', error);
            updateConfigStatus();
        });
}

function sendKernelActivationConfig() {
    try {
        sendConfigCommand({
            kernel: getKernelValues(),
            activations: getActivations()
        });
    } catch (error) {
        configStatus = error.message;
        updateConfigStatus();
    }
}

function setActiveTab(tab) {
    activeTab = tab;
    document.body.classList.toggle('config-active', tab === 'configure');
    document.getElementById('tab-topology').classList.toggle('active', tab === 'topology');
    document.getElementById('tab-configure').classList.toggle('active', tab === 'configure');
    onWindowResize();
}

// ============================================================================
// Event Handlers
// ============================================================================

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
    if (svg && zoomBehavior) {
        svg.transition()
            .duration(250)
            .call(zoomBehavior.transform, d3.zoomIdentity);
    }

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
    document.getElementById('tab-topology').addEventListener('click', () => setActiveTab('topology'));
    document.getElementById('tab-configure').addEventListener('click', () => setActiveTab('configure'));
    document.getElementById('show-labels').addEventListener('change', onShowLabelsChange);
    document.getElementById('show-rssi').addEventListener('change', onShowRssiChange);
    document.getElementById('min-rssi').addEventListener('input', onMinRssiChange);
    document.getElementById('reset-view').addEventListener('click', resetView);
    document.getElementById('pause-simulation').addEventListener('click', togglePause);
    document.getElementById('kernel-all-one').addEventListener('click', () => setKernelValues([1, 1, 1, 1, 1, 1, 1, 1, 1]));
    document.getElementById('kernel-all-zero').addEventListener('click', () => setKernelValues([0, 0, 0, 0, 0, 0, 0, 0, 0]));
    document.getElementById('kernel-neighbors').addEventListener('click', () => setKernelValues([1, 1, 1, 1, 1, 1, 1, 1, 0]));
    document.getElementById('activation-add').addEventListener('click', () => addActivation());
    document.getElementById('activation-remove').addEventListener('click', () => {
        const rows = document.querySelectorAll('.activation-row');
        if (rows.length > 0) rows[rows.length - 1].remove();
    });
    document.getElementById('activation-clear').addEventListener('click', () => {
        clearActivations();
        addActivation();
    });
    document.getElementById('config-send').addEventListener('click', sendKernelActivationConfig);
    initConfigEditor();
    
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
