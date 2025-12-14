/**
 * agent_task.c - Cloud communication task (Priority 1)
 *
 * Responsibility:
 * - Receive telemetry from alarm_task via queue
 * - Publish to MQTT when connected
 * - Buffer in ring_buffer when offline
 * - Flush buffer when MQTT reconnects
 *
 * Why Priority 1 (lowest)?
 * Cloud communication is NOT safety-critical. The sensor and alarm tasks
 * must never be blocked by slow network operations. (lecture W3L3)
 */

#include "agent_task.h"
#include "ring_buffer.h"
#include "mqtt_handler.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "AGENT";

// Queue handle - created here, receives data from alarm_task
QueueHandle_t telemetryQueue = NULL;

// Publish Telemetry_t to MQTT using new API
static void publish_telemetry(Telemetry_t *data)
{
    // Publish full telemetry
    mqtt_publish_telemetry(data->co_ppm, data->timestamp, data->state,
                           data->alarm_active, data->door_open);

    // Publish event if not just a regular reading
    if (strcmp(data->event, "READING") != 0) {
        mqtt_publish_event(data->event, data->co_ppm, data->timestamp);
    }

    ESP_LOGI(TAG, "Published: co=%.1f state=%d event=%s",
             data->co_ppm, data->state, data->event);
}

// Flush all buffered data to MQTT when reconnected
static void flush_ring_buffer(void)
{
    int flush_count = ring_buffer_count();
    if (flush_count == 0) {
        return;
    }

    ESP_LOGI(TAG, "MQTT reconnected, flushing %d items", flush_count);

    Telemetry_t item;
    int flushed = 0;

    while (ring_buffer_pop(&item)) {
        publish_telemetry(&item);
        flushed++;

        // Don't flood the broker - 20ms delay between publishes
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ESP_LOGI(TAG, "Flushed %d items to cloud", flushed);
}

/**
 * agent_task - Main task loop
 *
 * Runs forever at Priority 1 (lowest).
 * Receives from telemetryQueue, publishes or buffers based on MQTT state.
 */
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
                // Offline - store in ring buffer for later
                ring_buffer_push(&data);
                ESP_LOGW(TAG, "Offline, buffered. Count: %d", ring_buffer_count());
            }
        }

        // Track connection state for next iteration
        was_connected = is_connected;
    }
}

/**
 * agent_task_init - Initialize and start the agent task
 *
 * Creates:
 * - Ring buffer for offline storage
 * - Telemetry queue (alarm_task sends to this)
 * - Agent task at Priority 1
 */
void agent_task_init(void)
{
    // Initialize ring buffer for offline storage
    ring_buffer_init();

    // Create telemetry queue (receives from alarm_task)
    // Size 10 = can buffer 10 items before alarm_task blocks
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
