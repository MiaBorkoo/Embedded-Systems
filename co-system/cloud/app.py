"""
Flask + MQTT Dashboard for CO Safety System
CS4447 - Team: The Non-Functionals

Connects to MQTT broker at localhost:1883 (runs on alderaan)
Subscribes to ESP32 telemetry, publishes commands
"""

from flask import Flask
from collections import deque
import threading
import logging

# Import refactored modules
from parser import protocol_parser
from mqtt.mqtt_client import MQTTClientManager
from routes.api_routes import register_routes

app = Flask(__name__)

# Configure logging
logging.basicConfig(
    level=logging.INFO,  
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


# In-memory data storage
data_store = {
    "readings": deque(maxlen=100),
    "events": deque(maxlen=50),
    "device_status": {
        "state": 0,
        "armed": False,
        "last_update": None,
        "connected": False,
        "broker_connected": False
    },
    "current_reading": {
        "co_ppm": 0.0,
        "alarm": False,
        "door": False,
        "timestamp": 0
    },
    "lock": threading.Lock()
}

# Initialize MQTT client
mqtt_manager = MQTTClientManager(data_store, protocol_parser)

# Register all API routes
register_routes(app, data_store, mqtt_manager, protocol_parser)

# ============== Main ==============

if __name__ == "__main__":
    print("=" * 50)
    print("CO Safety System Dashboard")
    print("Team: The Non-Functionals")
    print("=" * 50)

    # Start MQTT in background
    mqtt_manager.start()

    # Run Flask (debug=False to prevent MQTT double-connection from reloader)
    app.run(host="0.0.0.0", port=8080, debug=False)
