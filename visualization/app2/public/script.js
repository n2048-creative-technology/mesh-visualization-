/**
 * Mesh Topology Visualization
 * Aesthetic: Cyberpunk digital projection with vertical data streams
 * Features: Dark immersive background, vertical blue/green glitch patterns,
 *          fragmented textures, horizontal scan lines, static noise, soft glow
 */

const WS_URL = `ws://${window.location.hostname}:${window.location.port}`;

let nodes = {};
let links = [];
let svg, viewport, zoomBehavior, simulation, nodeElements, linkElements, defs;
let ws;
let isPaused = false;
let isConnected = false;
let minRssi = -90;
let messageCount = 0;
let nodeCount = 0;

// Color palette: deep blues, teals, cyans
const COLORS = {
    background: '#040408',
    deepBlue: '#0a1628',
    teal: '#ecf0f0',
    cyan: '#0b0f10',
    cyanGlow: 'rgba(171, 175, 175, 0.5)',
    green: '#606563',
    greenGlow: 'rgba(18, 18, 18, 0.4)',
    nodeActive: '#ffffff',
    nodeInactive: '#0a1628',
    nodeBorder: '#959595',
    nodeBorderDim: '#141c1c',
    link: 'rgba(238, 233, 232, 0.8)',
    linkDim: 'rgba(0, 77, 77, 0.2)',
    scanLine: 'rgba(164, 168, 163, 0.3)',
    noise: 'rgba(99, 87, 85, 0.5)'
};

// ============================================================================
// Utility Functions
// ============================================================================

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

function getLinkDistance(rssi) {
    const clamped = Math.max(-95, Math.min(-25, rssi));
    const normalized = (clamped + 95) / 70;
    return 260 - normalized * 170;
}

function getLinkOpacity(rssi) {
    const clamped = Math.max(-100, Math.min(0, rssi));
    const normalized = (clamped + 100) / 100;
    return 0.1 + 0.5 * normalized;
}

function getNodeRadius(rssi) {
    if (rssi === undefined || rssi === null) return 2;
    const clamped = Math.max(-95, Math.min(-25, rssi));
    const normalized = (clamped + 95) / 70;
    return 1 + 10 * normalized;
}

function getAverageRssi(node) {
    if (!node || !node.neighbors || node.neighbors.length === 0) return -80;
    const sum = node.neighbors.reduce((acc, n) => acc + n.rssi, 0);
    return sum / node.neighbors.length;
}

// ============================================================================
// SVG Filters for Cyberpunk Effects
// ============================================================================

function createFilters() {
    defs = svg.append('defs');

    // Glow filter for nodes
    const glowFilter = defs.append('filter')
        .attr('id', 'glow')
        .attr('x', '-50%')
        .attr('y', '-50%')
        .attr('width', '200%')
        .attr('height', '200%');

    glowFilter.append('feGaussianBlur')
        .attr('stdDeviation', 2)
        .attr('result', 'coloredBlur');

    glowFilter.append('feMerge')
        .append('feMergeNode')
        .attr('in', 'coloredBlur');
    glowFilter.append('feMerge')
        .append('feMergeNode')
        .attr('in', 'SourceGraphic');

    // Scan line pattern
    const scanPattern = defs.append('pattern')
        .attr('id', 'scanlines')
        .attr('width', '100%')
        .attr('height', '4px')
        .attr('patternUnits', 'userSpaceOnUse');

    scanPattern.append('rect')
        .attr('width', '100%')
        .attr('height', '1px')
        .attr('fill', COLORS.scanLine);

    // Static noise filter
    const noiseFilter = defs.append('filter')
        .attr('id', 'noise')
        .attr('x', '0')
        .attr('y', '0')
        .attr('width', '100%')
        .attr('height', '100%');

    noiseFilter.append('feTurbulence')
        .attr('type', 'fractalNoise')
        .attr('baseFrequency', '0.04')
        .attr('numOctaves', '3')
        .attr('result', 'noise');

    noiseFilter.append('feColorMatrix')
        .attr('type', 'matrix')
        .attr('values', '0 0 0 0 0, 0 0 0 0 0, 0 0 0 0 0, 0 0 0 0.05 0');

    // Vertical data stream gradient
    const streamGradient = defs.append('linearGradient')
        .attr('id', 'stream-gradient')
        .attr('x1', '0%')
        .attr('y1', '0%')
        .attr('x2', '0%')
        .attr('y2', '100%');

    streamGradient.append('stop')
        .attr('offset', '0%')
        .attr('stop-color', 'rgba(0, 200, 255, 0.1)');

    streamGradient.append('stop')
        .attr('offset', '50%')
        .attr('stop-color', 'rgba(135, 139, 137, 0.5)');

    streamGradient.append('stop')
        .attr('offset', '100%')
        .attr('stop-color', 'rgba(255, 255, 255, 0.5)');

    // Glitch effect filter
    const glitchFilter = defs.append('filter')
        .attr('id', 'glitch')
        .attr('x', '-20%')
        .attr('y', '-20%')
        .attr('width', '140%')
        .attr('height', '140%');

    glitchFilter.append('feOffset')
        .attr('dx', '2')
        .attr('dy', '0')
        .attr('result', 'offset1');

    glitchFilter.append('feOffset')
        .attr('dx', '-2')
        .attr('dy', '0')
        .attr('result', 'offset2');

    glitchFilter.append('feMerge')
        .append('feMergeNode')
        .attr('in', 'offset1');
    glitchFilter.append('feMerge')
        .append('feMergeNode')
        .attr('in', 'offset2');
    glitchFilter.append('feMerge')
        .append('feMergeNode')
        .attr('in', 'SourceGraphic');
}

// ============================================================================
// Cyberpunk Background Layers
// ============================================================================

let dataStreams = [];
let scanLineLayer;
let noiseLayer;
let baseLayer;

function createBackground() {
    // Base dark background
    baseLayer = svg.append('rect')
        .attr('width', '100%')
        .attr('height', '100%')
        .attr('fill', COLORS.background);

    // Noise layer
    noiseLayer = svg.append('rect')
        .attr('width', '100%')
        .attr('height', '100%')
        .attr('fill', 'url(#noise)')
        .attr('filter', 'url(#noise)')
        .attr('opacity', 0.3);

    // Scan lines overlay
    scanLineLayer = svg.append('rect')
        .attr('width', '100%')
        .attr('height', '100%')
        .attr('fill', 'url(#scanlines)')
        .attr('opacity', 0.15);

    // Vertical data streams
    createDataStreams();
}

function createDataStreams() {
    dataStreams = [];
    const streamWidth = 20 + Math.random() * 40;
    const spacing = 80 + Math.random() * 60;
    const count = Math.ceil(window.innerWidth / spacing) + 1;

    for (let i = 0; i < count; i++) {
        const x = i * spacing + Math.random() * 40 - 20;
        const height = window.innerHeight;

        // Create stream group
        const streamGroup = svg.append('g')
            .attr('class', 'data-stream')
            .attr('transform', `translate(${x}, 0)`);

        // Main stream glow
        const glow = streamGroup.append('rect')
            .attr('x', -streamWidth / 2)
            .attr('y', 0)
            .attr('width', streamWidth)
            .attr('height', height)
            .attr('fill', 'url(#stream-gradient)')
            .attr('opacity', 0.3 + Math.random() * 0.2);

        // Animated fragments
        const fragmentCount = 15 + Math.floor(Math.random() * 10);
        const fragments = [];

        for (let j = 0; j < fragmentCount; j++) {
            const fragment = streamGroup.append('rect')
                .attr('x', -streamWidth / 2 + Math.random() * streamWidth)
                .attr('y', Math.random() * height)
                .attr('width', 2 + Math.random() * 8)
                .attr('height', 10 + Math.random() * 40)
                .attr('fill', j % 2 === 0 ? COLORS.cyan : COLORS.green)
                .attr('opacity', 0.4 + Math.random() * 0.4);

            fragments.push({
                element: fragment,
                baseY: Math.random() * height,
                speed: 0.5 + Math.random() * 2,
                height: 10 + Math.random() * 40,
                opacity: 0.4 + Math.random() * 0.4
            });
        }

        dataStreams.push({
            x: x,
            width: streamWidth,
            glow: glow,
            fragments: fragments,
            baseOpacity: 0.3 + Math.random() * 0.2
        });
    }
}

function updateBackground() {
    if (!svg) return;

    const totalNodes = Object.keys(nodes).length;
    const time = Date.now() * 0.001;
    const intensity = Math.min(totalNodes / 10, 1);
    const activity = Math.min(messageCount / 50, 1);

    // Update scan lines
    scanLineLayer.attr('opacity', 0.1 + 0.1 * intensity);

    // Update noise
    noiseLayer.attr('opacity', 0.2 + 0.15 * activity);

    // Update data streams
    dataStreams.forEach((stream, streamIndex) => {
        const streamPulse = Math.sin(time * 0.3 + streamIndex * 0.5) * 0.5 + 0.5;
        const streamIntensity = (0.5 + 0.5 * streamPulse) * intensity;

        // Update glow
        stream.glow.attr('opacity', stream.baseOpacity * streamIntensity);

        // Update fragments
        stream.fragments.forEach((fragment, fragIndex) => {
            const y = (fragment.baseY + time * 100 * fragment.speed) % window.innerHeight;
            const fragPulse = Math.sin(time * 2 + fragIndex * 0.3 + streamIndex * 0.7) * 0.5 + 0.5;
            const opacity = fragment.opacity * streamIntensity * (0.7 + 0.3 * fragPulse);
            const height = fragment.height * (0.8 + 0.2 * Math.sin(time + fragIndex * 0.5));

            fragment.element
                .attr('y', y)
                .attr('height', height)
                .attr('opacity', opacity);
        });
    });
}

// ============================================================================
// D3.js Setup - Mesh Visualization
// ============================================================================

function initVisualization() {
    svg = d3.select('#visualization');
    
    createFilters();
    createBackground();
    
    viewport = svg.append('g').attr('class', 'viewport');
    zoomBehavior = d3.zoom()
        .scaleExtent([0.1, 10])
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
        .force('link', d3.forceLink().id(d => d.mac).distance(d => getLinkDistance(d.rssi)).strength(0.5))
        .force('charge', d3.forceManyBody().strength(-300).distanceMax(600))
        .force('center', d3.forceCenter(window.innerWidth / 2, window.innerHeight / 2))
        .force('collision', d3.forceCollide().radius(d => getNodeRadius(getAverageRssi(d)) + 4))
        .velocityDecay(0.4)
        .alphaDecay(0.02);

    const linkContainer = viewport.append('g').attr('class', 'links');
    const nodeContainer = viewport.append('g').attr('class', 'nodes');

    function updateLinks() {
        linkElements = linkContainer.selectAll('.link')
            .data(links, d => `${sourceMac(d)}-${targetMac(d)}`);

        linkElements.enter()
            .append('line')
            .attr('class', 'link')
            .attr('stroke-width', 0.7)
            .attr('stroke', COLORS.link)
            .attr('stroke-linecap', 'round')
            .attr('opacity', d => getLinkOpacity(d.rssi))
            .merge(linkElements);

        linkElements
            .attr('stroke', d => {
                const isStrong = d.rssi > -60;
                return isStrong ? COLORS.cyan : COLORS.linkDim;
            })
            .attr('opacity', d => getLinkOpacity(d.rssi));

        linkElements.exit().remove();
    }

    function updateNodes() {
        const nodeSelection = nodeContainer.selectAll('.node')
            .data(Object.values(nodes), d => d.mac);

        const newNodes = nodeSelection.enter()
            .append('circle')
            .attr('class', 'node')
            .attr('r', d => getNodeRadius(getAverageRssi(d)))
            .attr('stroke', COLORS.nodeBorderDim)
            .attr('stroke-width', 1)
            .attr('fill', COLORS.nodeInactive)
            .attr('filter', 'url(#glow)');

        nodeElements = newNodes.merge(nodeSelection)
            .attr('class', d => {
                const isActive = d.value === 1 || d.state === 2;
                return `node ${isActive ? 'active' : 'inactive'}`;
            })
            .attr('r', d => getNodeRadius(getAverageRssi(d)))
            .attr('stroke', d => {
                const isActive = d.value === 1 || d.state === 2;
                return isActive ? COLORS.nodeBorder : COLORS.nodeBorderDim;
            })
            .attr('stroke-width', 1)
            .attr('fill', d => {
                const isActive = d.value === 1 || d.state === 2;
                return isActive ? COLORS.nodeActive : COLORS.nodeInactive;
            });

        nodeSelection.exit().remove();
    }

    function updateSimulation() {
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

        simulation
            .nodes(Object.values(nodes))
            .on('tick', ticked);

        simulation.force('link')
            .distance(d => getLinkDistance(d.rssi))
            .links(links);

        simulation.force('collision')
            .radius(d => getNodeRadius(getAverageRssi(d)) + 4);

        simulation.alphaTarget(0.08).restart();
        window.clearTimeout(updateSimulation.alphaTimer);
        updateSimulation.alphaTimer = window.setTimeout(() => {
            simulation.alphaTarget(0);
        }, 1400);
    }

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
    }

    updateSimulation();
    updateLinks();
    updateNodes();

    return {
        updateLinks,
        updateNodes,
        updateSimulation,
        ticked
    };
}

// ============================================================================
// WebSocket Connection
// ============================================================================

function connectWebSocket() {
    try {
        ws = new WebSocket(WS_URL);

        ws.onopen = () => {
            isConnected = true;
            console.log('WebSocket connected');
        };

        ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            messageCount++;

            switch (data.type) {
                case 'full_state':
                    nodes = {};
                    for (const node of data.data) {
                        nodes[node.mac] = mergeNodeData(nodes[node.mac], node);
                    }
                    nodeCount = Object.keys(nodes).length;
                    updateVisualization();
                    break;

                case 'node_update':
                    nodes[data.data.mac] = mergeNodeData(nodes[data.data.mac], data.data);
                    nodeCount = Object.keys(nodes).length;
                    updateVisualization();
                    break;

                case 'stats':
                    nodeCount = data.data.nodeCount || nodeCount;
                    break;

                default:
                    console.log('Unknown message type:', data.type);
            }
        };

        ws.onclose = () => {
            isConnected = false;
            console.log('WebSocket disconnected');
            setTimeout(connectWebSocket, 5000);
        };

        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            isConnected = false;
        };

    } catch (error) {
        console.error('Failed to create WebSocket:', error);
        isConnected = false;
    }
}

// ============================================================================
// Visualization Update
// ============================================================================

let visualization;

function updateVisualization() {
    if (!visualization || isPaused) return;
    visualization.updateSimulation();
    visualization.updateLinks();
    visualization.updateNodes();
}

// ============================================================================
// Window Events
// ============================================================================

function onWindowResize() {
    svg.attr('width', window.innerWidth)
       .attr('height', window.innerHeight);

    if (simulation) {
        simulation.force('center', d3.forceCenter(window.innerWidth / 2, window.innerHeight / 2));
    }

    // Recreate background elements on resize
    svg.selectAll('.data-stream').remove();
    createDataStreams();
    
    updateVisualization();
}

// ============================================================================
// Animation Loop
// ============================================================================

function startAnimationLoop() {
    function animate() {
        updateBackground();
        requestAnimationFrame(animate);
    }
    animate();
}

// ============================================================================
// Initialization
// ============================================================================

function init() {
    visualization = initVisualization();
    connectWebSocket();

    window.addEventListener('resize', onWindowResize);
    onWindowResize();

    startAnimationLoop();

    console.log('Cyberpunk mesh visualization initialized');
}

document.addEventListener('DOMContentLoaded', init);
