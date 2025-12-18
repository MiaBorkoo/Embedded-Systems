"""
API Routes for CO Safety System Dashboard
Handles all HTTP endpoints for data retrieval and command sending
"""

from flask import render_template, jsonify, request
from datetime import datetime
import paho.mqtt.client as mqtt

# State names for display
STATE_NAMES = {0: "INIT", 1: "NORMAL", 2: "OPEN", 3: "EMERGENCY"}


def register_routes(app, data_store, mqtt_manager, protocol_parser):
    """
    Register all Flask routes for the dashboard
    
    Args:
        app: Flask application instance
        data_store: Dictionary containing readings, events, device_status, etc.
        mqtt_manager: MQTT client manager instance
        protocol_parser: Protocol parser module with command constants
    """
    
    @app.route("/")
    def index():
        """Serve the dashboard page"""
        return render_template("index.html")

    @app.route("/api/readings")
    def get_readings():
        """Return recent CO readings"""
        with data_store["lock"]:
            return jsonify({
                "readings": list(data_store["readings"]),
                "current": data_store["current_reading"].copy()
            })

    @app.route("/api/events")
    def get_events():
        """Return recent events with pagination"""
        page = request.args.get('page', 1, type=int)
        per_page = request.args.get('per_page', 10, type=int)

        # Clamp per_page to reasonable limits
        per_page = max(5, min(per_page, 50))

        with data_store["lock"]:
            all_events = list(data_store["events"])

        # Reverse to show newest first
        all_events = all_events[::-1]

        total = len(all_events)
        total_pages = (total + per_page - 1) // per_page if total > 0 else 1

        # Clamp page to valid range
        page = max(1, min(page, total_pages))

        # Slice for current page
        start = (page - 1) * per_page
        end = start + per_page
        page_events = all_events[start:end]

        return jsonify({
            "events": page_events,
            "pagination": {
                "page": page,
                "per_page": per_page,
                "total": total,
                "total_pages": total_pages,
                "has_prev": page > 1,
                "has_next": page < total_pages
            }
        })

    @app.route("/api/status")
    def get_status():
        """Return current device status"""
        with data_store["lock"]:
            status = data_store["device_status"].copy()
            status["state_name"] = STATE_NAMES.get(status["state"], "UNKNOWN")
            status["current"] = data_store["current_reading"].copy()
            return jsonify(status)

    @app.route("/api/command", methods=["POST"])
    def send_command():
        """Send command to device via MQTT"""
        data = request.get_json()
        if not isinstance(data, dict):
            return jsonify({"error": "Invalid or missing JSON in request body"}), 400
        command = data.get("command", "").upper()

        valid_commands = ["START_EMER", "STOP_EMER", "TEST", "OPEN_DOOR"]
        if command not in valid_commands:
            return jsonify({"error": f"Invalid command. Valid: {valid_commands}"}), 400

        # Map command string to ID
        command_id_map = {
            "START_EMER": protocol_parser.CMD_START_EMER,
            "STOP_EMER": protocol_parser.CMD_STOP_EMER,
            "TEST": protocol_parser.CMD_TEST,
            "OPEN_DOOR": protocol_parser.CMD_OPEN_DOOR
        }
        command_id = command_id_map[command]

        # Build binary packet
        packet = protocol_parser.build_command_packet(command_id)

        # Publish to MQTT
        from mqtt.mqtt_client import TOPIC_COMMANDS
        success = mqtt_manager.publish_command(TOPIC_COMMANDS, packet, qos=1)

        if success:
            # Log the command as an event
            with data_store["lock"]:
                data_store["events"].append({
                    "event": f"CMD_{command}",
                    "co_ppm": data_store["current_reading"]["co_ppm"],
                    "timestamp": 0,
                    "received_at": datetime.now().isoformat()
                })
            return jsonify({"success": True, "command": command})
        else:
            return jsonify({"error": "Failed to publish command"}), 500
