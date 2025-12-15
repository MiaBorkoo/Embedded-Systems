"""
Flask + MQTT Dashboard for CO Safety System
CS4447 - Team: The Non-Functionals

Connects to MQTT broker at localhost:1883 (runs on alderaan)
Subscribes to ESP32 telemetry, publishes commands
"""

from flask import Flask, render_template, jsonify, request
from collections import deque
from datetime import datetime
import threading
import logging
import paho.mqtt.client as mqtt
from protocol_parser import (
    parse_packet,
    build_command_packet,
    CMD_ARM,
    CMD_DISARM,
    CMD_TEST,
    CMD_RESET,
    CMD_OPEN_DOOR,
    PROTOCOL_START_MARKER,
    PROTOCOL_END_MARKER
)

app = Flask(__name__)

# Configure logging
logging.basicConfig(
    level=logging.INFO,  
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


# In-memory storage (last 100 of each)
readings = deque(maxlen=100)
events = deque(maxlen=50)
device_status = {
    "state": 0,
    "armed": False,
    "last_update": None,
    "connected": False,
    "broker_connected": False  # Tracks MQTT broker connection (not device)
}
current_reading = {
    "co_ppm": 0.0,
    "alarm": False,
    "door": False,
    "timestamp": 0
}

# MQTT Topics
TOPIC_CO = "nonfunctionals/sensors/co"
TOPIC_EVENTS = "nonfunctionals/events/door"
TOPIC_STATUS = "nonfunctionals/status"
TOPIC_COMMANDS = "nonfunctionals/commands"

# State names for display
STATE_NAMES = {0: "SAFE", 1: "WARNING", 2: "ALARM", 3: "DISARMED"}

# Thread lock for data access
data_lock = threading.Lock()


# ============== MQTT Callbacks ==============

def on_connect(client, userdata, flags, reason_code, properties):
    """Called when connected to MQTT broker"""
    print(f"[MQTT] Connected with result code: {reason_code}")
    # Subscribe to all ESP32 topics
    client.subscribe(TOPIC_CO)
    client.subscribe(TOPIC_EVENTS)
    client.subscribe(TOPIC_STATUS)
    print(f"[MQTT] Subscribed to topics")
    with data_lock:
        device_status["connected"] = True
        device_status["broker_connected"] = True


def on_disconnect(client, userdata, flags, reason_code, properties):
    """Called when disconnected from MQTT broker"""
    print(f"[MQTT] Disconnected with result code: {reason_code}")
    with data_lock:
        device_status["connected"] = False
        device_status["broker_connected"] = False


def on_message(client, userdata, msg):
    payload = msg.payload

    if len(payload) < 6:
        logger.warning("Packet too short — dropped")
        return

    if payload[0] != PROTOCOL_START_MARKER or payload[-1] != PROTOCOL_END_MARKER:
        logger.warning("Invalid binary markers — dropped")
        return

    parsed = parse_packet(payload)
    if parsed is None:
        logger.warning("Binary packet failed validation — dropped")
        return

    packet_type = parsed["packet_type"]

    with data_lock:
        device_status["connected"] = True
        device_status["last_update"] = parsed["received_at"]

    if packet_type == "telemetry":
        with data_lock:
            readings.append(parsed)
            current_reading.update({
                "co_ppm": parsed["co_ppm"],
                "alarm": parsed["alarm_active"],
                "door": parsed["door_open"],
                "timestamp": parsed["timestamp"]
            })
            device_status["state"] = parsed["state"]

        logger.info(
            f"Telemetry | CO={parsed['co_ppm']} Alarm={parsed['alarm_active']} Door={parsed['door_open']}"
        )

    elif packet_type == "event":
        with data_lock:
            events.append(parsed)
        logger.info(f"Event | {parsed['event']}")
    
    elif packet_type == "status":
        with data_lock:
            device_status["armed"] = parsed["armed"]
            device_status["state"] = parsed["state"]
            device_status["last_update"] = parsed["received_at"]

    else:
        logger.warning(f"Unhandled packet type: {packet_type}")


# ============== MQTT Client Setup ==============

mqtt_client = mqtt.Client(
    callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    client_id="nonfunctionals-dashboard"
)
mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_message = on_message


def start_mqtt():
    """Start MQTT client in background thread"""
    try:
        mqtt_client.connect("localhost", 1883, 60)
        mqtt_client.loop_start()
        print("[MQTT] Client started")
    except Exception as e:
        print(f"[MQTT] Connection failed: {e}")
        print("[MQTT] WARNING: Dashboard will not receive data or send commands!")
        with data_lock:
            device_status["broker_connected"] = False


# ============== Flask Routes ==============

@app.route("/")
def index():
    """Serve the dashboard page"""
    return render_template("index.html")


@app.route("/api/readings")
def get_readings():
    """Return recent CO readings"""
    with data_lock:
        return jsonify({
            "readings": list(readings),
            "current": current_reading.copy()
        })


@app.route("/api/events")
def get_events():
    """Return recent events"""
    with data_lock:
        return jsonify({"events": list(events)})


@app.route("/api/status")
def get_status():
    """Return current device status"""
    with data_lock:
        status = device_status.copy()
        status["state_name"] = STATE_NAMES.get(status["state"], "UNKNOWN")
        status["current"] = current_reading.copy()
        return jsonify(status)


@app.route("/api/command", methods=["POST"])
def send_command():
    """Send command to device via MQTT"""
    data = request.get_json()
    if not isinstance(data, dict):
        return jsonify({"error": "Invalid or missing JSON in request body"}), 400
    command = data.get("command", "").upper()

    valid_commands = ["ARM", "DISARM", "TEST", "RESET", "OPEN_DOOR"]
    if command not in valid_commands:
        return jsonify({"error": f"Invalid command. Valid: {valid_commands}"}), 400
    

    # Map command string to ID
    command_id_map = {
        "ARM": CMD_ARM,
        "DISARM": CMD_DISARM,
        "TEST": CMD_TEST,
        "RESET": CMD_RESET,
        "OPEN_DOOR": CMD_OPEN_DOOR
    }
    command_id = command_id_map[command]

    # Build binary packet
    packet = build_command_packet(command_id)

    # Publish to MQTT
    result = mqtt_client.publish(TOPIC_COMMANDS, packet, qos=1)

    if result.rc == mqtt.MQTT_ERR_SUCCESS:
        # Log the command as an event
        with data_lock:
            events.append({
                "event": f"CMD_{command}",
                "co_ppm": current_reading["co_ppm"],
                "timestamp": 0,
                "received_at": datetime.now().isoformat()
            })
        return jsonify({"success": True, "command": command})
    else:
        return jsonify({"error": "Failed to publish command"}), 500

# ============== Main ==============

if __name__ == "__main__":
    print("=" * 50)
    print("CO Safety System Dashboard")
    print("Team: The Non-Functionals")
    print("=" * 50)

    # Start MQTT in background
    start_mqtt()

    # Run Flask (debug=False for production on alderaan)
    app.run(host="0.0.0.0", port=5000, debug=True)
