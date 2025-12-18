"""
MQTT Client Module for CO Safety System
Handles connection, subscriptions, and message processing
"""

import paho.mqtt.client as mqtt
import logging
from datetime import datetime

logger = logging.getLogger(__name__)

# MQTT Topics
TOPIC_CO = "nonfunctionals/sensors/co"
TOPIC_STATUS = "nonfunctionals/status"
TOPIC_COMMANDS = "nonfunctionals/commands"


class MQTTClientManager:
    """Manages MQTT connection and message handling for the dashboard"""
    
    def __init__(self, data_store, protocol_parser):
        """
        Initialize MQTT client manager
        
        Args:
            data_store: Dictionary containing readings, events, device_status, etc.
            protocol_parser: Module with parse_packet and protocol constants
        """
        self.data_store = data_store
        self.protocol_parser = protocol_parser
        
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="nonfunctionals-dashboard"
        )
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
    
    def _on_connect(self, client, userdata, flags, reason_code, properties):
        """Called when connected to MQTT broker"""
        print(f"[MQTT] Connected with result code: {reason_code}")
        # Subscribe to ESP32 topics
        client.subscribe(TOPIC_CO)
        client.subscribe(TOPIC_STATUS)
        print(f"[MQTT] Subscribed to topics")
        with self.data_store["lock"]:
            self.data_store["device_status"]["connected"] = True
            self.data_store["device_status"]["broker_connected"] = True
    
    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        """Called when disconnected from MQTT broker"""
        print(f"[MQTT] Disconnected with result code: {reason_code}")
        with self.data_store["lock"]:
            self.data_store["device_status"]["connected"] = False
            self.data_store["device_status"]["broker_connected"] = False
    
    def _on_message(self, client, userdata, msg):
        """Process incoming MQTT messages"""
        payload = msg.payload

        if len(payload) < 6:
            logger.warning("Packet too short — dropped")
            return

        if payload[0] != self.protocol_parser.PROTOCOL_START_MARKER or \
           payload[-1] != self.protocol_parser.PROTOCOL_END_MARKER:
            logger.warning("Invalid binary markers — dropped")
            return

        parsed = self.protocol_parser.parse_packet(payload)
        if parsed is None:
            logger.warning("Binary packet failed validation — dropped")
            return

        packet_type = parsed["packet_type"]

        with self.data_store["lock"]:
            self.data_store["device_status"]["connected"] = True
            self.data_store["device_status"]["last_update"] = parsed["received_at"]

        if packet_type == "telemetry":
            with self.data_store["lock"]:
                self.data_store["readings"].append(parsed)
                self.data_store["current_reading"].update({
                    "co_ppm": parsed["co_ppm"],
                    "alarm": parsed["alarm_active"],
                    "door": parsed["door_open"],
                    "timestamp": parsed["timestamp"]
                })
                self.data_store["device_status"]["state"] = parsed["state"]

            logger.info(
                f"Telemetry | CO={parsed['co_ppm']} Alarm={parsed['alarm_active']} Door={parsed['door_open']}"
            )

        elif packet_type == "event":
            with self.data_store["lock"]:
                self.data_store["events"].append(parsed)
            logger.info(f"Event | {parsed['event']}")
        
        elif packet_type == "status":
            with self.data_store["lock"]:
                self.data_store["device_status"]["armed"] = parsed["armed"]
                self.data_store["device_status"]["state"] = parsed["state"]
                self.data_store["device_status"]["last_update"] = parsed["received_at"]

        else:
            logger.warning(f"Unhandled packet type: {packet_type}")
    
    def start(self, host="localhost", port=1883, keepalive=60):
        """Start MQTT client in background thread"""
        try:
            self.client.connect(host, port, keepalive)
            self.client.loop_start()
            print("[MQTT] Client started")
        except Exception as e:
            print(f"[MQTT] Connection failed: {e}")
            print("[MQTT] WARNING: Dashboard will not receive data or send commands!")
            with self.data_store["lock"]:
                self.data_store["device_status"]["broker_connected"] = False
    
    def publish_command(self, topic, packet, qos=1):
        """Publish command packet to MQTT broker"""
        result = self.client.publish(topic, packet, qos=qos)
        return result.rc == mqtt.MQTT_ERR_SUCCESS
