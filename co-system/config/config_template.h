#ifndef CONFIG_H
#define CONFIG_H

/**
 * CO Safety System Configuration Template
 * ========================================
 * 
 * TO USE THIS TEMPLATE:
 * 1. Copy this file to config/config.h
 * 2. Update the values below with your actual settings
 * 3. config/config.h is gitignored and won't be committed
 * 
 * This template is safe to commit to version control.
 */

// ============== WiFi Configuration ==============
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"
#define WIFI_MAX_RETRY      3
#define WIFI_TIMEOUT_MS     10000
#define WIFI_RECONNECT_MS   5000

// ============== MQTT Configuration ==============
#define MQTT_BROKER_URI     "mqtt://your-broker.example.com:1883"
#define MQTT_CLIENT_ID      "your-device-id"

// MQTT Topics
#define MQTT_TOPIC_CO       "your-team/sensors/co"
#define MQTT_TOPIC_EVENTS   "your-team/events/door"
#define MQTT_TOPIC_STATUS   "your-team/status"
#define MQTT_TOPIC_COMMANDS "your-team/commands"

// ============== Hardware Pin Configuration ==============
// Door/Servo
#define SERVO_PIN           25
#define GREEN_LED_PIN       26
#define RED_LED_PIN         27
#define BUTTON_PIN          12

// Buzzer
#define BUZZER_PIN          14

// Sensor
#define CO_SENSOR_PIN       34  // ADC1 Channel 6

// ============== System Thresholds ==============
#define CO_THRESHOLD_ALARM_PPM      35.0f   // Trigger emergency state
#define CO_THRESHOLD_SENSOR_PPM     100.0f  // Sensor reading threshold

// ============== Timing Configuration ==============
#define INIT_DURATION_MS            3000    // Self-test duration
#define DOOR_OPEN_DURATION_MS       5000    // Auto-close delay
#define BUZZER_BEEP_INTERVAL_MS     300     // Beep on/off interval
#define SENSOR_READING_INTERVAL_MS  500     // Sensor poll rate
#define STATUS_LOG_INTERVAL_MS      2000    // Main loop status log

// ============== Task Configuration ==============
#define TASK_PRIORITY_SENSOR        10      // Highest (safety-critical)
#define TASK_PRIORITY_FSM           5       // Medium
#define TASK_PRIORITY_AGENT         1       // Lowest (cloud comms)

#define TASK_STACK_SENSOR           2048
#define TASK_STACK_FSM              4096
#define TASK_STACK_AGENT            4096
#define TASK_STACK_BUZZER           2048

// ============== Queue Configuration ==============
#define QUEUE_SIZE_TELEMETRY        10
#define QUEUE_SIZE_COMMAND          10
#define QUEUE_SIZE_FSM_EVENT        10

// ============== Ring Buffer Configuration ==============
#define RING_BUFFER_SIZE_CONFIG     100     // Offline telemetry storage

// ============== Servo Configuration ==============
#define SERVO_FREQ_HZ               50
#define SERVO_MIN_PULSE_US          500
#define SERVO_MAX_PULSE_US          2400

// ============== Buzzer Configuration ==============
#define BUZZER_FREQ_HZ              1000    // Tone frequency

// ============== Button Configuration ==============
#define BUTTON_DEBOUNCE_MS          200

// ============== IFTTT Webhook Configuration ==============
#define IFTTT_ENABLED               1       // 0=disabled, 1=enabled
#define IFTTT_WEBHOOK_URL           "https://maker.ifttt.com/trigger/emergency/json/with/key/YOUR_KEY"
#define IFTTT_TIMEOUT_MS            5000    // HTTP request timeout
#define TASK_PRIORITY_IFTTT         1       // Same as agent (low priority)
#define TASK_STACK_IFTTT            4096    // Stack for HTTP client
#define QUEUE_SIZE_IFTTT            2       // Webhook trigger queue

#endif // CONFIG_H
