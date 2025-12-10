#include "mqtt_handler.h"
#include "shared_types.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"

// MQTT Broker configuration
#define MQTT_BROKER_URI "mqtt://200.69.13.70:1883"

// MQTT Topics
#define TOPIC_CO        "nonfunctionals/sensors/co"
#define TOPIC_DOOR      "nonfunctionals/events/door"
#define TOPIC_STATUS    "nonfunctionals/status"
#define TOPIC_COMMANDS  "nonfunctionals/commands"

static const char *TAG = "MQTT";

// MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

// External queue for commands (defined in main.c)
extern QueueHandle_t commandQueue;

// Parse incoming command JSON and send to queue
static void parse_command(const char *data, int len)
{
    // Null-terminate the data
    char *json_str = malloc(len + 1);
    if (!json_str) return;
    memcpy(json_str, data, len);
    json_str[len] = '\0';

    ESP_LOGI(TAG, "Received command: %s", json_str);

    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        ESP_LOGW(TAG, "Failed to parse command JSON");
        free(json_str);
        return;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(json, "command");
    if (cJSON_IsString(cmd_item)) {
        Command_t cmd = CMD_NONE;
        const char *cmd_str = cmd_item->valuestring;

        if (strcmp(cmd_str, "OPEN_DOOR") == 0) {
            cmd = CMD_OPEN_DOOR;
        } else if (strcmp(cmd_str, "RESET") == 0) {
            cmd = CMD_RESET;
        } else if (strcmp(cmd_str, "TEST") == 0) {
            cmd = CMD_TEST;
        }

        if (cmd != CMD_NONE && commandQueue != NULL) {
            xQueueSend(commandQueue, &cmd, 0);
            ESP_LOGI(TAG, "Command sent to queue: %s", cmd_str);
        }
    }

    cJSON_Delete(json);
    free(json_str);
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
            // Subscribe to commands topic
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
            // Check if this is from commands topic
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

    // Last Will Testament - published if we disconnect unexpectedly
    const char *lwt_msg = "{\"state\":\"OFFLINE\"}";

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = MQTT_BROKER_URI,
            },
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

void mqtt_publish_co(float co_level, uint32_t timestamp)
{
    if (!mqtt_connected || mqtt_client == NULL) return;

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"co\":%.2f,\"ts\":%lu}", co_level, (unsigned long)timestamp);

    // QoS 0 for sensor data (fire and forget)
    esp_mqtt_client_publish(mqtt_client, TOPIC_CO, payload, 0, 0, 0);
}

void mqtt_publish_door_event(const char *event_type, float co_level, uint32_t timestamp)
{
    if (!mqtt_connected || mqtt_client == NULL) return;

    char payload[128];
    if (co_level >= 0) {
        // Include CO level for emergency events
        snprintf(payload, sizeof(payload), "{\"event\":\"%s\",\"co\":%.2f,\"ts\":%lu}",
                 event_type, co_level, (unsigned long)timestamp);
    } else {
        snprintf(payload, sizeof(payload), "{\"event\":\"%s\",\"ts\":%lu}",
                 event_type, (unsigned long)timestamp);
    }

    // QoS 1 for door events (guaranteed delivery)
    esp_mqtt_client_publish(mqtt_client, TOPIC_DOOR, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published door event: %s", event_type);
}

void mqtt_publish_status(const char *state)
{
    if (!mqtt_connected || mqtt_client == NULL) return;

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"state\":\"%s\"}", state);

    // QoS 1, retained for status
    esp_mqtt_client_publish(mqtt_client, TOPIC_STATUS, payload, 0, 1, 1);
    ESP_LOGI(TAG, "Published status: %s", state);
}
