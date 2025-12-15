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
import json
import paho.mqtt.client as mqtt

app = Flask(__name__)

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
    """Handle incoming MQTT messages"""
    try:
        payload = json.loads(msg.payload.decode())
        topic = msg.topic

        with data_lock:
            if topic == TOPIC_CO:
                # CO sensor reading
                reading = {
                    "co_ppm": payload.get("co_ppm", 0),
                    "timestamp": payload.get("timestamp", 0),
                    "state": payload.get("state", 0),
                    "alarm": payload.get("alarm", False),
                    "door": payload.get("door", False),
                    "received_at": datetime.now().isoformat()
                }
                readings.append(reading)
                current_reading.update({
                    "co_ppm": reading["co_ppm"],
                    "alarm": reading["alarm"],
                    "door": reading["door"],
                    "timestamp": reading["timestamp"]
                })
                device_status["state"] = reading["state"]
                device_status["last_update"] = reading["received_at"]

            elif topic == TOPIC_EVENTS:
                # Door/alarm events
                event = {
                    "event": payload.get("event", "UNKNOWN"),
                    "co_ppm": payload.get("co_ppm", 0),
                    "timestamp": payload.get("timestamp", 0),
                    "received_at": datetime.now().isoformat()
                }
                events.append(event)

            elif topic == TOPIC_STATUS:
                # Device status update
                device_status["state"] = payload.get("state", 0)
                device_status["armed"] = payload.get("armed", False)
                device_status["last_update"] = datetime.now().isoformat()

    except json.JSONDecodeError as e:
        print(f"[MQTT] JSON decode error: {e}")
    except Exception as e:
        print(f"[MQTT] Error processing message: {e}")


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

    payload = json.dumps({"command": command})
    result = mqtt_client.publish(TOPIC_COMMANDS, payload, qos=1)

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
