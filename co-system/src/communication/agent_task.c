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
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "protocol.h"
#include "stats/stats.h"

static const char *TAG = "AGENT";

// Queue handle - created here, receives data from alarm_task
QueueHandle_t telemetryQueue = NULL;

// Publish Telemetry_t to MQTT using new API
static void publish_telemetry(const Telemetry_t *telemetry) {
    if (!telemetry) {
        return;
    }

    // Always publish telemetry packet
    uint8_t telemetry_packet[PROTOCOL_MAX_PACKET_SIZE];
    size_t telemetry_len = 0;

    if (protocol_encode_telemetry(telemetry, telemetry_packet, &telemetry_len)) {
        // Publish binary packet to MQTT
        bool success = mqtt_publish_raw(
            MQTT_TOPIC_CO,               // Topic
            telemetry_packet,            // Binary data
            telemetry_len,               // Length
            0                            // QoS 0
        );

        if (success) {
            stats_record_telemetry_sent();
            ESP_LOGD(TAG, "Published telemetry packet (%zu bytes)", telemetry_len);
           // protocol_print_packet(telemetry_packet, telemetry_len, TAG);  // Debug
        } else {
            stats_record_telemetry_buffered();
            ESP_LOGW(TAG, "Failed to publish telemetry packet");
        }
    }

    // If this is an event (not just "READING"), also send event packet
    if (strncmp(telemetry->event, "READING", sizeof(telemetry->event)) != 0) {
        uint8_t event_packet[PROTOCOL_MAX_PACKET_SIZE];
        size_t event_len = 0;

        if (protocol_encode_event(telemetry, event_packet, &event_len)) {
            bool success = mqtt_publish_raw(
                MQTT_TOPIC_STATUS,       // Or TOPIC_DOOR for door events
                event_packet,
                event_len,
                1                        // QoS 1 for events
            );

            if (success) {
                stats_record_event_sent();
                ESP_LOGD(TAG, "Published event packet: %s (%zu bytes)", 
                         telemetry->event, event_len);
                //protocol_print_packet(event_packet, event_len, TAG);  //debug
            } else {
                ESP_LOGW(TAG, "Failed to publish event packet");
            }
        }
    }
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

    ESP_LOGI(TAG, "Agent task started (Priority %d)", TASK_PRIORITY_AGENT);

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
    telemetryQueue = xQueueCreate(QUEUE_SIZE_TELEMETRY, sizeof(Telemetry_t));
    if (telemetryQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create telemetry queue");
        return;
    }

    // Create the agent task at Priority 1 (lowest)
    BaseType_t ret = xTaskCreate(
        agent_task,
        "agent_task",
        TASK_STACK_AGENT,
        NULL,
        TASK_PRIORITY_AGENT,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create agent task");
        return;
    }

    ESP_LOGI(TAG, "Agent task initialized");
}
