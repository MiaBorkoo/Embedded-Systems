/**
 * mqtt_handler.c - MQTT client for cloud communication
 *
 * Connects to alderaan.software-engineering.ie:1883
 * Publishes telemetry, events, status
 * Subscribes to commands and sends parsed commands to commandQueue
 */

#include "mqtt_handler.h"
#include "shared_types.h"
#include "protocol.h"


#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "MQTT";

// MQTT Broker configuration
#define MQTT_BROKER_URI  "mqtt://alderaan.software-engineering.ie:1883"
#define MQTT_CLIENT_ID   "nonfunctionals-esp32"

// MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

// External queue for commands (defined in main.c)
extern QueueHandle_t commandQueue;

/**
 * Parse incoming BINARY command packet using protocol decoder
 */
static void parse_command(const char *data, int len)
{
    // Cast to uint8_t for binary parsing
    const uint8_t *binary_data = (const uint8_t *)data;

    ESP_LOGI(TAG, "Received command packet (%d bytes)", len);
    
    // Optional: Print hex dump for debugging
    if (len <= 20) {
        char hex_str[128];
        int offset = 0;
        for (int i = 0; i < len && offset < sizeof(hex_str) - 4; i++) {
            offset += snprintf(&hex_str[offset], sizeof(hex_str) - offset,
                             "%02X ", binary_data[i]);
        }
        ESP_LOGD(TAG, "Command packet hex: %s", hex_str);
    }

    // Decode command packet using protocol.c
    uint8_t command_id = 0;
    
    if (!protocol_decode_command(binary_data, len, &command_id)) {
        ESP_LOGE(TAG, "Failed to decode command packet");
        return;
    }

    // Map command ID to Command_t enum
    Command_t cmd = CMD_NONE;
    
    switch (command_id) {
        case 0x01:
            cmd = CMD_ARM;
            ESP_LOGI(TAG, "Command: ARM");
            break;
        case 0x02:
            cmd = CMD_DISARM;
            ESP_LOGI(TAG, "Command: DISARM");
            break;
        case 0x03:
            cmd = CMD_RESET;
            ESP_LOGI(TAG, "Command: RESET");
            break;
        case 0x04:
            cmd = CMD_OPEN_DOOR;
            ESP_LOGI(TAG, "Command: OPEN_DOOR");
            break;
        default:
            ESP_LOGW(TAG, "Unknown command ID: 0x%02X", command_id);
            return;
    }

    // Send to command queue
    if (cmd != CMD_NONE && commandQueue != NULL) {
        if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdPASS) {
            ESP_LOGI(TAG, "Command sent to queue successfully");
        } else {
            ESP_LOGW(TAG, "Failed to send command to queue (queue full?)");
        }
    }
}

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker");
            mqtt_connected = true;
            esp_mqtt_client_subscribe(mqtt_client, TOPIC_COMMANDS, 1);
            ESP_LOGI(TAG, "Subscribed to %s", TOPIC_COMMANDS);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            mqtt_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Subscribed to topic, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            if (event->topic_len > 0 && event->data_len > 0) {
                if (strncmp(event->topic, TOPIC_COMMANDS, event->topic_len) == 0) {
                    parse_command(event->data, event->data_len);
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error occurred");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP transport error");
            }
            break;

        default:
            break;
    }
}

void mqtt_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT client...");

    const char *lwt_msg = "{\"state\":0,\"armed\":false}";

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = MQTT_BROKER_URI,
            },
        },
        .credentials = {
            .client_id = MQTT_CLIENT_ID,
        },
        .session = {
            .last_will = {
                .topic = TOPIC_STATUS,
                .msg = lwt_msg,
                .msg_len = strlen(lwt_msg),
                .qos = 1,
                .retain = true,
            },
        },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "MQTT client started, connecting to %s", MQTT_BROKER_URI);
}

bool mqtt_is_connected(void)
{
    return mqtt_connected;
}

// Publish telemetry: {"co_ppm": 45.5, "timestamp": 12345, "state": 0, "alarm": false, "door": false}
bool mqtt_publish_telemetry(float co_ppm, uint32_t timestamp, uint8_t state,
                            bool alarm_active, bool door_open)
{
    if (!mqtt_connected || mqtt_client == NULL) return false;

    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"co_ppm\":%.2f,\"timestamp\":%lu,\"state\":%d,\"alarm\":%s,\"door\":%s}",
             co_ppm, (unsigned long)timestamp, state,
             alarm_active ? "true" : "false",
             door_open ? "true" : "false");

    int msg_id = esp_mqtt_client_publish(mqtt_client, TOPIC_CO, payload, 0, 0, 0);
    return (msg_id >= 0);
}

// Publish event: {"event": "ALARM_ON", "co_ppm": 85.2, "timestamp": 12345}
bool mqtt_publish_event(const char *event_type, float co_ppm, uint32_t timestamp)
{
    if (!mqtt_connected || mqtt_client == NULL) return false;

    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"event\":\"%s\",\"co_ppm\":%.2f,\"timestamp\":%lu}",
             event_type, co_ppm, (unsigned long)timestamp);

    int msg_id = esp_mqtt_client_publish(mqtt_client, TOPIC_DOOR, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published event: %s", event_type);
    return (msg_id >= 0);
}

// Publish status: {"state": 0, "armed": true}
bool mqtt_publish_status(uint8_t state, bool armed)
{
    if (!mqtt_connected || mqtt_client == NULL) return false;

    char payload[64];
    snprintf(payload, sizeof(payload),
             "{\"state\":%d,\"armed\":%s}",
             state, armed ? "true" : "false");

    int msg_id = esp_mqtt_client_publish(mqtt_client, TOPIC_STATUS, payload, 0, 1, 1);
    ESP_LOGI(TAG, "Published status: state=%d armed=%s", state, armed ? "true" : "false");
    return (msg_id >= 0);
}

// Publish raw binary data (for teammate's protocol)
bool mqtt_publish_raw(const char *topic, const uint8_t *data, size_t len, int qos)
{
    if (!mqtt_connected || mqtt_client == NULL) return false;

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, (const char *)data, len, qos, 0);
    return (msg_id >= 0);
}
