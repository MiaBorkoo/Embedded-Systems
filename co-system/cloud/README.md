# CO Safety System - Cloud Dashboard

Flask + MQTT dashboard for the Smart CO Safety System.

**Team:** The Non-Functionals
**Module:** CS4447 - IoT Embedded Systems

## Setup on Alderaan

```bash
# SSH into alderaan
ssh yourusername@alderaan.software-engineering.ie

# Navigate to your directory
cd /path/to/your/project/cloud

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Run the dashboard
python app.py
```

The dashboard will be available at: `http://alderaan.software-engineering.ie:5000`

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `nonfunctionals/sensors/co` | ESP32 -> Cloud | CO readings |
| `nonfunctionals/status` | ESP32 -> Cloud | Events and device status |
| `nonfunctionals/commands` | Cloud -> ESP32 | Commands |

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Dashboard page |
| `/api/readings` | GET | Recent CO readings |
| `/api/events` | GET | Recent events |
| `/api/status` | GET | Device status |
| `/api/command` | POST | Send command |

## Commands

Send via POST to `/api/command` with JSON body:

```json
{"command": "START_EMER"}
```

Valid commands: `START_EMER`, `STOP_EMER`, `TEST`, `OPEN_DOOR`

## Running in Background

To keep the dashboard running after logout:

```bash
# Using nohup
nohup python app.py > dashboard.log 2>&1 &

# Or using screen
screen -S dashboard
python app.py
# Press Ctrl+A, then D to detach
# Reattach with: screen -r dashboard
```

## Files

```
cloud/
├── app.py              # Flask + MQTT backend
├── templates/
│   └── index.html      # Dashboard page
├── static/
│   ├── css/
│   │   └── style.css   # Custom styles
│   └── js/
│       └── dashboard.js # Chart and API logic
├── requirements.txt    # Python dependencies
└── README.md           # This file
```
