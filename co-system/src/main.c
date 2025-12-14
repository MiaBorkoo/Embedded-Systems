#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "communication/wifi_manager.h"
#include "communication/mqtt_handler.h"
#include "communication/agent_task.h"
#include "communication/ring_buffer.h"
#include "shared_types.h"

static const char *TAG = "MAIN";

// Queue for commands from MQTT (cloud -> device)
QueueHandle_t commandQueue = NULL;

// System state
static bool system_armed = true;  // Start armed

void app_main(void)
{
    ESP_LOGI(TAG, "CO Safety System starting...");

    // Create command queue (for commands from cloud)
    commandQueue = xQueueCreate(10, sizeof(Command_t));

    // Initialize WiFi
    wifi_init();

    // Wait for WiFi to connect before starting MQTT
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    while (!wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "WiFi connected!");

    // Initialize MQTT
    mqtt_init();

    // Wait for MQTT to connect (with timeout)
    ESP_LOGI(TAG, "Waiting for MQTT connection...");
    int mqtt_wait = 0;
    while (!mqtt_is_connected() && mqtt_wait < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        mqtt_wait++;
    }

    if (mqtt_is_connected()) {
        ESP_LOGI(TAG, "MQTT connected!");
        mqtt_publish_status(0, system_armed);  // state=0 (SAFE), armed=true
    } else {
        ESP_LOGW(TAG, "MQTT connection timeout, continuing anyway...");
    }

    // Initialize agent task (creates telemetryQueue and ring buffer)
    agent_task_init();

    ESP_LOGI(TAG, "All systems initialized!");
    ESP_LOGI(TAG, "Task Priorities: sensor=3, alarm=2, agent=1");

    // Test: Simulate sending telemetry data (normally alarm_task does this)
    uint32_t counter = 0;
    float test_co = 10.0;

    while (1) {
        // Check for incoming commands from cloud
        Command_t cmd;
        if (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
            switch (cmd) {
                case CMD_ARM:
                    ESP_LOGI(TAG, ">>> Received ARM command!");
                    system_armed = true;
                    mqtt_publish_status(0, system_armed);
                    break;
                case CMD_DISARM:
                    ESP_LOGI(TAG, ">>> Received DISARM command!");
                    system_armed = false;
                    mqtt_publish_status(3, system_armed);  // state=3 (DISARMED)
                    break;
                case CMD_OPEN_DOOR:
                    ESP_LOGI(TAG, ">>> Received OPEN_DOOR command!");
                    // TODO: Call door_open_request() when integrated
                    break;
                case CMD_RESET:
                    ESP_LOGI(TAG, ">>> Received RESET command!");
                    // TODO: Reset alarm state when integrated
                    break;
                case CMD_TEST:
                    ESP_LOGI(TAG, ">>> Received TEST command!");
                    // TODO: Trigger test alarm when integrated
                    break;
                default:
                    break;
            }
        }

        // Simulate telemetry from alarm_task (for testing)
        // In real system, alarm_task sends this via xQueueSend(telemetryQueue, ...)
        Telemetry_t test_data = {
            .timestamp = (uint32_t)(esp_timer_get_time() / 1000),
            .co_ppm = test_co,
            .alarm_active = (test_co > 35.0),
            .door_open = false,
            .state = (test_co > 35.0) ? 2 : 0,  // 0=SAFE, 2=ALARM
            .event = "READING"
        };

        // Send to agent_task via telemetryQueue
        if (telemetryQueue != NULL) {
            xQueueSend(telemetryQueue, &test_data, 0);
        }

        // Simulate varying CO levels
        test_co += 0.5;
        if (test_co > 50.0) test_co = 10.0;

        // Status log
        ESP_LOGI(TAG, "WiFi: %s | MQTT: %s | Buffer: %d | Armed: %s | Count: %lu",
                 wifi_is_connected() ? "OK" : "NO",
                 mqtt_is_connected() ? "OK" : "NO",
                 ring_buffer_count(),
                 system_armed ? "YES" : "NO",
                 (unsigned long)counter++);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
