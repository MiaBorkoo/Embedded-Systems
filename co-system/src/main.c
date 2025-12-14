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
#include "fsm/fsm.h"
#include "door_task/door.h"
#include "buzzer_task/buzzer.h"
#include "emergency_state/emergency.h"
#include "sensor_task/sensor.h"

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

    // Initialize hardware
    door_init();
    buzzer_init();
    emergency_init();
    sensor_init();

    // Initialize FSM (after hardware and queues are ready)
    fsm_init();

    ESP_LOGI(TAG, "All systems initialized!");
    ESP_LOGI(TAG, "Task Priorities: sensor=10, fsm=5, agent=1");

    // Main loop - handle MQTT commands and monitor system
    uint32_t counter = 0;

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
                    door_open_request();
                    break;
                case CMD_RESET:
                    ESP_LOGI(TAG, ">>> Received RESET command!");
                    if (fsmEventQueue != NULL) {
                        FSMEvent_t event = {
                            .type = EVENT_CMD_RESET,
                            .co_ppm = 0.0f
                        };
                        xQueueSend(fsmEventQueue, &event, 0);
                    }
                    break;
                case CMD_TEST:
                    ESP_LOGI(TAG, ">>> Received TEST command!");
                    if (fsmEventQueue != NULL) {
                        FSMEvent_t event = {
                            .type = EVENT_CMD_TEST,
                            .co_ppm = 0.0f
                        };
                        xQueueSend(fsmEventQueue, &event, 0);
                    }
                    break;
                default:
                    break;
            }
        }

        // Status log
        SystemState_t current_state = fsm_get_state();
        const char *state_names[] = {"NORMAL", "OPEN", "EMERGENCY"};
        ESP_LOGI(TAG, "WiFi: %s | MQTT: %s | Buffer: %d | State: %s | Count: %lu",
                 wifi_is_connected() ? "OK" : "NO",
                 mqtt_is_connected() ? "OK" : "NO",
                 ring_buffer_count(),
                 state_names[current_state],
                 (unsigned long)counter++);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
