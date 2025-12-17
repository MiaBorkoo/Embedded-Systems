# Smart CO Safety System

**Team:** The Non-Functionals
**Module:** CS4447 - IoT Embedded Systems

**Authors:**
- Mia Borko (@MiaBorkoo)
- Ruan O'Dowd (@ruqn)
- Alisia K (@alisiak04)

A real-time Carbon Monoxide safety system built on ESP32 with FreeRTOS.

## Architecture

```
┌─────────────┐     MQTT      ┌─────────────┐     HTTP      ┌─────────────┐
│   ESP32     │──────────────▶│   Alderaan  │◀────────────▶│  Dashboard  │
│  FreeRTOS   │◀──────────────│   Broker    │              │   (Flask)   │
└─────────────┘   Commands    └─────────────┘              └─────────────┘
```

## Features

- **Real-time CO monitoring** with MQ-7 sensor
- **FreeRTOS multi-tasking** with priority-based scheduling
- **Binary protocol** with CRC8 checksum
- **Offline buffering** - stores up to 100 readings when disconnected
- **Web dashboard** for monitoring and control

## FreeRTOS Tasks

| Task | Priority | Purpose |
|------|----------|---------|
| sensor_task | 10 | CO sensor readings (safety-critical) |
| fsm_task | 5 | State machine control |
| buzzer_task | 5 | Alarm sound generation |
| agent_task | 1 | Cloud communication |

## System States

| State | Description |
|-------|-------------|
| INIT | 3-second self-test on boot |
| NORMAL | Normal operation, monitoring CO |
| OPEN | Door open for ventilation |
| EMERGENCY | CO alarm active |

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `nonfunctionals/sensors/co` | Device → Cloud | CO telemetry |
| `nonfunctionals/status` | Device → Cloud | Status/LWT |
| `nonfunctionals/commands` | Cloud → Device | Commands |

## Commands

| Command | ID | Description |
|---------|-----|-------------|
| START_EMER | 0x01 | Start emergency mode |
| STOP_EMER | 0x02 | Stop emergency mode |
| TEST | 0x03 | 3-second alarm test |
| OPEN_DOOR | 0x04 | Open door for ventilation |

## Project Structure

```
co-system/
├── src/
│   ├── main.c                 # Entry point, command handling
│   ├── fsm/                   # Finite State Machine
│   ├── sensor_task/           # CO sensor reading
│   ├── buzzer_task/           # Alarm buzzer control
│   ├── door_task/             # Servo door + button
│   ├── communication/         # MQTT, WiFi, protocol
│   └── emergency_state/       # Emergency mode logic
├── include/
│   └── shared_types.h         # Common type definitions
└── cloud/
    ├── app.py                 # Flask + MQTT backend
    ├── protocol_parser.py     # Binary protocol decoder
    └── templates/index.html   # Dashboard UI
```

## Quick Start

### ESP32
```bash
cd co-system
idf.py build
idf.py flash monitor
```

### Cloud Dashboard (on Alderaan)
```bash
cd cloud
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python app.py
```

Dashboard: `http://alderaan.software-engineering.ie/node`

## Hardware

- ESP32 FireBeetle
- MQ-7 CO Sensor
- Servo motor (door)
- Passive buzzer
- Push button
- Red/Green LEDs
