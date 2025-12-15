/**
 * CO Safety System Dashboard
 * Handles API polling, chart updates, and command sending
 */

// CO Thresholds (ppm)
const CO_WARNING = 35;
const CO_DANGER = 70;
const CO_MAX_DISPLAY = 100;

// Chart instance
let coChart = null;

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    initChart();
    fetchStatus();
    fetchEvents();

    // Poll for updates every 2 seconds
    setInterval(fetchStatus, 2000);
    setInterval(fetchEvents, 5000);
});

/**
 * Initialize the CO level chart
 */
function initChart() {
    const ctx = document.getElementById('co-chart').getContext('2d');

    coChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'CO (ppm)',
                data: [],
                borderColor: '#22c55e',
                backgroundColor: 'rgba(34, 197, 94, 0.1)',
                borderWidth: 2,
                fill: true,
                tension: 0.3,
                pointRadius: 0,
                pointHoverRadius: 4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: {
                duration: 300
            },
            scales: {
                x: {
                    display: true,
                    grid: {
                        color: 'rgba(75, 85, 99, 0.3)'
                    },
                    ticks: {
                        color: '#9ca3af',
                        maxTicksLimit: 10
                    }
                },
                y: {
                    display: true,
                    min: 0,
                    max: CO_MAX_DISPLAY,
                    grid: {
                        color: 'rgba(75, 85, 99, 0.3)'
                    },
                    ticks: {
                        color: '#9ca3af'
                    }
                }
            },
            plugins: {
                legend: {
                    display: false
                },
                tooltip: {
                    backgroundColor: '#1f2937',
                    titleColor: '#f3f4f6',
                    bodyColor: '#d1d5db',
                    borderColor: '#374151',
                    borderWidth: 1
                }
            }
        }
    });
}

/**
 * Fetch current status and readings from API
 */
async function fetchStatus() {
    try {
        const response = await fetch('/api/readings');
        const data = await response.json();

        updateDashboard(data);
        updateChart(data.readings);

    } catch (error) {
        console.error('Error fetching status:', error);
    }

    // Also fetch device status
    try {
        const statusResponse = await fetch('/api/status');
        const status = await statusResponse.json();
        updateConnectionStatus(status);
    } catch (error) {
        console.error('Error fetching device status:', error);
    }
}

/**
 * Update dashboard UI with current data
 */
function updateDashboard(data) {
    const current = data.current;
    const coValue = current.co_ppm || 0;

    // Update CO value display
    const coValueEl = document.getElementById('co-value');
    coValueEl.textContent = coValue.toFixed(1);

    // Set color based on level
    coValueEl.classList.remove('co-safe', 'co-warning', 'co-danger');
    if (coValue >= CO_DANGER) {
        coValueEl.classList.add('co-danger');
    } else if (coValue >= CO_WARNING) {
        coValueEl.classList.add('co-warning');
    } else {
        coValueEl.classList.add('co-safe');
    }

    // Update CO bar
    const coBar = document.getElementById('co-bar');
    const barPercent = Math.min((coValue / CO_MAX_DISPLAY) * 100, 100);
    coBar.style.width = barPercent + '%';
    coBar.classList.remove('bar-safe', 'bar-warning', 'bar-danger');
    if (coValue >= CO_DANGER) {
        coBar.classList.add('bar-danger');
    } else if (coValue >= CO_WARNING) {
        coBar.classList.add('bar-warning');
    } else {
        coBar.classList.add('bar-safe');
    }

    // Update alarm indicator
    const alarmIndicator = document.getElementById('alarm-indicator');
    const alarmText = document.getElementById('alarm-text');
    if (current.alarm) {
        alarmIndicator.className = 'w-4 h-4 rounded-full bg-red-500 alarm-active';
        alarmText.textContent = 'ACTIVE';
        alarmText.className = 'text-2xl font-bold text-red-500';
    } else {
        alarmIndicator.className = 'w-4 h-4 rounded-full bg-green-500';
        alarmText.textContent = 'OFF';
        alarmText.className = 'text-2xl font-bold text-green-500';
    }

    // Update door indicator
    const doorIndicator = document.getElementById('door-indicator');
    const doorText = document.getElementById('door-text');
    if (current.door) {
        doorIndicator.className = 'w-4 h-4 rounded-full bg-purple-500';
        doorText.textContent = 'OPEN';
        doorText.className = 'text-2xl font-bold text-purple-500';
    } else {
        doorIndicator.className = 'w-4 h-4 rounded-full bg-gray-500';
        doorText.textContent = 'CLOSED';
        doorText.className = 'text-2xl font-bold text-gray-400';
    }
}

/**
 * Update connection status indicator
 */
function updateConnectionStatus(status) {
    const statusDot = document.getElementById('status-dot');
    const statusText = document.getElementById('status-text');

    if (status.connected) {
        statusDot.className = 'w-3 h-3 rounded-full status-connected';
        statusText.textContent = 'Connected';
    } else {
        statusDot.className = 'w-3 h-3 rounded-full status-disconnected';
        statusText.textContent = 'Disconnected';
    }

    // Update state display
    const stateValue = document.getElementById('state-value');
    const armedStatus = document.getElementById('armed-status');

    stateValue.textContent = status.state_name || '--';
    stateValue.classList.remove('co-safe', 'co-warning', 'co-danger');

    if (status.state === 2) {
        stateValue.classList.add('co-danger');
    } else if (status.state === 1) {
        stateValue.classList.add('co-warning');
    } else if (status.state === 3) {
        stateValue.className = 'text-3xl font-bold text-yellow-500';
    } else {
        stateValue.classList.add('co-safe');
    }

    armedStatus.textContent = status.armed ? 'System Armed' : 'System Disarmed';
    armedStatus.className = status.armed ? 'text-sm text-green-500 mt-2' : 'text-sm text-yellow-500 mt-2';

    // Update last update time
    if (status.last_update) {
        const date = new Date(status.last_update);
        document.getElementById('last-update').textContent = date.toLocaleTimeString();
    }
}

/**
 * Update chart with new readings
 */
function updateChart(readings) {
    if (!coChart || !readings || readings.length === 0) return;

    // Get last 50 readings for chart
    const recentReadings = readings.slice(-50);

    const labels = recentReadings.map(r => {
        const date = new Date(r.received_at);
        return date.toLocaleTimeString('en-US', {
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        });
    });

    const values = recentReadings.map(r => r.co_ppm);

    // Update chart color based on latest value
    const latestValue = values[values.length - 1] || 0;
    let borderColor = '#22c55e';
    let bgColor = 'rgba(34, 197, 94, 0.1)';

    if (latestValue >= CO_DANGER) {
        borderColor = '#ef4444';
        bgColor = 'rgba(239, 68, 68, 0.1)';
    } else if (latestValue >= CO_WARNING) {
        borderColor = '#eab308';
        bgColor = 'rgba(234, 179, 8, 0.1)';
    }

    coChart.data.labels = labels;
    coChart.data.datasets[0].data = values;
    coChart.data.datasets[0].borderColor = borderColor;
    coChart.data.datasets[0].backgroundColor = bgColor;
    coChart.update('none');
}

/**
 * Fetch recent events
 */
async function fetchEvents() {
    try {
        const response = await fetch('/api/events');
        const data = await response.json();
        updateEventsTable(data.events);
    } catch (error) {
        console.error('Error fetching events:', error);
    }
}

/**
 * Update events table
 */
function updateEventsTable(events) {
    const tbody = document.getElementById('events-table');

    if (!events || events.length === 0) {
        tbody.innerHTML = '<tr><td colspan="3" class="text-center py-4 text-gray-500">No events yet</td></tr>';
        return;
    }

    // Show most recent first, limit to 20
    const recentEvents = events.slice(-20).reverse();

    tbody.innerHTML = recentEvents.map(event => {
        const time = new Date(event.received_at).toLocaleTimeString();
        const eventType = event.event || 'UNKNOWN';

        // Determine badge class
        let badgeClass = 'event-normal';
        if (eventType.includes('ALARM')) badgeClass = 'event-alarm';
        else if (eventType.includes('DOOR')) badgeClass = 'event-door';
        else if (eventType.includes('CMD')) badgeClass = 'event-cmd';

        return `
            <tr class="border-b border-gray-700">
                <td class="py-2 px-3 text-gray-400">${time}</td>
                <td class="py-2 px-3">
                    <span class="px-2 py-1 rounded text-xs font-medium ${badgeClass}">${eventType}</span>
                </td>
                <td class="py-2 px-3">${event.co_ppm.toFixed(1)} ppm</td>
            </tr>
        `;
    }).join('');
}

/**
 * Send command to device
 */
async function sendCommand(command) {
    const feedback = document.getElementById('command-feedback');

    try {
        feedback.textContent = 'Sending...';
        feedback.className = 'mt-4 text-sm text-center text-gray-400';
        feedback.classList.remove('hidden');

        const response = await fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ command: command })
        });

        const data = await response.json();

        if (response.ok) {
            feedback.textContent = `Command "${command}" sent!`;
            feedback.className = 'mt-4 text-sm text-center text-green-500';
        } else {
            feedback.textContent = data.error || 'Failed to send command';
            feedback.className = 'mt-4 text-sm text-center text-red-500';
        }

        // Hide feedback after 3 seconds
        setTimeout(() => {
            feedback.classList.add('hidden');
        }, 3000);

        // Refresh events
        setTimeout(fetchEvents, 500);

    } catch (error) {
        console.error('Error sending command:', error);
        feedback.textContent = 'Network error';
        feedback.className = 'mt-4 text-sm text-center text-red-500';
    }
}
