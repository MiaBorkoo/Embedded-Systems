# Smart CO Safety System

**Team:** The Non-Functionals
**Module:** CS4447 - IoT Embedded Systems (fun times hihi)

**Authors:**
- Mia Borko (@MiaBorkoo)
- Ruan O'Dowd (@ruqn)
- Alisia Kazimierek (@alisiak04)

A real-time Carbon Monoxide safety system built on ESP32 with FreeRTOS.

## Architecture

```
┌─────────────┐     MQTT      ┌─────────────┐     HTTP      ┌─────────────┐
│   ESP32     │──────────────▶│   Alderaan  │◀────────────▶│  Dashboard  │
│  FreeRTOS   │◀──────────────│   Broker    │              │   (Flask)   │
└─────────────┘   Commands    └─────────────┘              └─────────────┘
                                    │
                                    ▼
                              ┌─────────────┐
                              │    IFTTT    │
                              │   Webhook   │
                              └─────────────┘
```

## Features

- **Real-time CO monitoring** with MQ-7 sensor (10 ppm emergency threshold)
- **FreeRTOS multi-tasking** with priority-based scheduling
- **Binary protocol** with CRC8 checksum
- **Offline buffering** - ring buffer stores up to 100 readings when disconnected
- **Web dashboard** for monitoring and control
- **IFTTT webhook** - emergency notifications via HTTP webhook

## FreeRTOS Tasks

| Task | Priority | Purpose |
|------|----------|---------|
| sensor_task | 5 | CO sensor readings (safety-critical) |
| fsm_task | 5 | State machine control (safety-critical) |
| buzzer_task | 4 | Alarm sound generation |
| agent_task | 2 | MQTT cloud communication |
| ifttt_task | 2 | IFTTT webhook triggers |
| stats_task | 1 | Runtime statistics monitoring |

Priority groups: Safety (5), Output (4), Cloud (2), Monitor (1)

## System States

| State | Description |
|-------|-------------|
| INIT | 3-second self-test on boot (all outputs active) |
| NORMAL | Normal operation, monitoring CO levels |
| OPEN | Door open for ventilation (5-second auto-close) |
| EMERGENCY | CO alarm active (buzzer, red LED, door open) |

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `nonfunctionals/sensors/co` | Device → Cloud | CO telemetry |
| `nonfunctionals/events/door` | Device → Cloud | State change events |
| `nonfunctionals/status` | Device → Cloud | Device status |
| `nonfunctionals/commands` | Cloud → Device | Remote commands |

## Commands

| Command | ID | Description |
|---------|-----|-------------|
| START_EMER | 0x01 | Start emergency mode |
| STOP_EMER | 0x02 | Stop emergency mode |
| TEST | 0x03 | 3-second alarm test |
| OPEN_DOOR | 0x04 | Open door for ventilation |

## Binary Protocol

All MQTT messages use a custom binary protocol:

```
[START 0xAA][TYPE][LENGTH][PAYLOAD...][CRC8][END 0x55]
```

- **Telemetry (0x01)**: timestamp, CO ppm, flags, state
- **Event (0x02)**: timestamp, CO ppm, event name, flags, state
- **Status (0x03)**: armed flag, state
- **Command (0x10)**: command ID

## Project Structure

```
co-system/
├── config/
│   ├── config.h              # System configuration (gitignored)
│   └── config_template.h     # Configuration template
├── src/
│   ├── main.c                # Entry point, command handling
│   ├── fsm/                  # Finite State Machine
│   ├── sensor_task/          # CO sensor reading
│   ├── buzzer_task/          # Alarm buzzer control
│   ├── door_task/            # Servo door + button
│   ├── communication/        # MQTT, WiFi, ring buffer, IFTTT
│   ├── emergency_state/      # Emergency mode logic
│   └── stats/                # Runtime statistics
├── include/
│   └── shared_types.h        # Common type definitions
└── cloud/
    ├── app.py                # Flask + MQTT backend
    ├── protocol_parser.py    # Binary protocol decoder
    ├── templates/index.html  # Dashboard UI
    └── static/               # CSS and JavaScript
```

## Configuration

Copy `config/config_template.h` to `config/config.h` and update:

- WiFi credentials
- MQTT broker URI
- Hardware pin assignments
- IFTTT webhook key

## Quick Start

### ESP32
```bash
cd co-system
cp config/config_template.h config/config.h
# Edit config/config.h with your settings
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

| Component | GPIO Pin |
|-----------|----------|
| MQ-7 CO Sensor | GPIO34 (ADC) |
| Servo Motor | GPIO27 |
| Buzzer | GPIO18 |
| Button | GPIO26 |
| Red LED | GPIO16 |
| Green LED | GPIO4 |

## Thresholds

| Parameter | Value |
|-----------|-------|
| CO Emergency | 10 ppm |
| CO Warning (UI) | 8 ppm |
| Init Duration | 3 seconds |
| Door Auto-close | 5 seconds |
| Sensor Poll Rate | 500 ms |
