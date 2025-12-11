#include "agent_task.h"
#include "ring_buffer.h"
#include "communication/mqtt_handler.h"
#include "shared_types.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "AGENT";

// Queue handle - created here, receives data from alarm_task
QueueHandle_t telemetryQueue = NULL;

// Convert Telemetry_t (from queue) to RingBufferItem_t (for storage)
static void telemetry_to_buffer_item(Telemetry_t *src, RingBufferItem_t *dst)
{
    dst->timestamp = src->timestamp;
    dst->co_ppm = src->co_level;
    dst->alarm_active = (src->state == STATE_ALARM);
    dst->door_open = (src->door_event == EVENT_MANUAL_OPEN ||
                      src->door_event == EVENT_EMERGENCY_OPEN);

    // Set event string based on state
    if (src->state_changed) {
        switch (src->state) {
            case STATE_ALARM:
                strncpy(dst->event, "ALARM", sizeof(dst->event) - 1);
                break;
            case STATE_SAFE:
                strncpy(dst->event, "SAFE", sizeof(dst->event) - 1);
                break;
            case STATE_COOLDOWN:
                strncpy(dst->event, "COOLDOWN", sizeof(dst->event) - 1);
                break;
            default:
                strncpy(dst->event, "READING", sizeof(dst->event) - 1);
                break;
        }
    } else {
        strncpy(dst->event, "READING", sizeof(dst->event) - 1);
    }
    dst->event[sizeof(dst->event) - 1] = '\0';
}

// Publish RingBufferItem_t to MQTT
static void publish_buffer_item(RingBufferItem_t *data)
{
    // Publish CO data
    mqtt_publish_co(data->co_ppm, data->timestamp);

    ESP_LOGI(TAG, "Published: co=%.1f alarm=%d door=%d event=%s",
             data->co_ppm, data->alarm_active, data->door_open, data->event);
}

// Publish Telemetry_t directly to MQTT (when online)
static void publish_telemetry(Telemetry_t *data)
{
    mqtt_publish_co(data->co_level, data->timestamp);

    // Publish door events if any
    if (data->door_event != EVENT_NONE) {
        const char *event_str = "unknown";
        switch (data->door_event) {
            case EVENT_MANUAL_OPEN:
                event_str = "manual_open";
                break;
            case EVENT_EMERGENCY_OPEN:
                event_str = "emergency_open";
                break;
            case EVENT_CLOSED:
                event_str = "closed";
                break;
            default:
                break;
        }
        mqtt_publish_door_event(event_str, data->co_level, data->timestamp);
    }

    // Publish status change if any
    if (data->state_changed) {
        const char *state_str = "SAFE";
        switch (data->state) {
            case STATE_ALARM:
                state_str = "ALARM";
                break;
            case STATE_COOLDOWN:
                state_str = "COOLDOWN";
                break;
            case STATE_DISARMED:
                state_str = "DISARMED";
                break;
            default:
                state_str = "SAFE";
                break;
        }
        mqtt_publish_status(state_str);
    }

    ESP_LOGI(TAG, "Published: co=%.1f state=%d", data->co_level, data->state);
}

// Flush all buffered data to MQTT
static void flush_ring_buffer(void)
{
    int flush_count = ring_buffer_count();
    if (flush_count == 0) {
        return;
    }

    ESP_LOGI(TAG, "MQTT reconnected, flushing %d items", flush_count);

    RingBufferItem_t item;
    int flushed = 0;

    while (ring_buffer_pop(&item)) {
        publish_buffer_item(&item);
        flushed++;

        // Don't flood the broker - 20ms delay between publishes
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ESP_LOGI(TAG, "Flushed %d items to cloud", flushed);
}

// Main agent task - runs at Priority 1 (lowest)
// Why lowest? Cloud comms is NOT safety-critical (lecture W3L3)
// Sensor (priority 3) and Alarm (priority 2) must never be blocked by network
void agent_task(void *params)
{
    bool was_connected = false;
    Telemetry_t data;

    ESP_LOGI(TAG, "Agent task started (Priority %d)", AGENT_TASK_PRIORITY);

    while (1) {
        bool is_connected = mqtt_is_connected();

        // Step 1: Detect reconnection - flush buffer first
        if (is_connected && !was_connected) {
            flush_ring_buffer();
        }

        // Step 2: Check for new telemetry from alarm_task
        // Use 100ms timeout - don't block forever so we can check connection state
        if (xQueueReceive(telemetryQueue, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (is_connected) {
                // Online - publish immediately
                publish_telemetry(&data);
            } else {
                // Offline - convert and store in ring buffer for later
                RingBufferItem_t item;
                telemetry_to_buffer_item(&data, &item);
                ring_buffer_push(&item);
                ESP_LOGW(TAG, "Offline, buffered. Count: %d", ring_buffer_count());
            }
        }

        // Track connection state for next iteration
        was_connected = is_connected;
    }
}

// Initialize and start the agent task
void agent_task_init(void)
{
    // Initialize ring buffer for offline storage
    ring_buffer_init();

    // Create telemetry queue (receives from alarm_task)
    telemetryQueue = xQueueCreate(10, sizeof(Telemetry_t));
    if (telemetryQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create telemetry queue");
        return;
    }

    // Create the agent task at Priority 1 (lowest)
    BaseType_t ret = xTaskCreate(
        agent_task,
        "agent_task",
        AGENT_TASK_STACK,
        NULL,
        AGENT_TASK_PRIORITY,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create agent task");
        return;
    }

    ESP_LOGI(TAG, "Agent task initialized");
}
