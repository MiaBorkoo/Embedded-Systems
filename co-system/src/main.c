#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "config.h"
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

#ifndef UNIT_TEST
void app_main(void)
{
    ESP_LOGI(TAG, "CO Safety System starting...");

    // Create command queue (for commands from cloud)
    commandQueue = xQueueCreate(QUEUE_SIZE_COMMAND, sizeof(Command_t));

    // Initialize agent task FIRST (creates telemetryQueue and ring buffer)
    agent_task_init();

    // Initialize FSM (creates mutex and queue before high-priority tasks need them)
    // FSM starts in STATE_INIT for 3-second self-test
    fsm_init();

    // Initialize sensors/tasks that depend on FSM
    // Self-test runs while WiFi connects in background
    door_init();
    buzzer_init();
    emergency_init();
    sensor_init();

    ESP_LOGI(TAG, "Hardware initialized! Starting 3-second self-test...");
    ESP_LOGI(TAG, "Task Priorities: sensor=10, fsm=5, agent=1");

    // Initialize WiFi (non-blocking - connects in background during self-test)
    wifi_init();

    // Wait for WiFi to connect before starting MQTT (DNS resolution needs WiFi)
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    int wifi_wait_count = 0;
    while (!wifi_is_connected() && wifi_wait_count < 60) {  // Wait up to 30 seconds
        vTaskDelay(pdMS_TO_TICKS(500));
        wifi_wait_count++;
    }
    
    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "WiFi connected! Starting MQTT...");
        // Initialize MQTT (now WiFi is ready for DNS resolution)
        mqtt_init();
    } else {
        ESP_LOGW(TAG, "WiFi connection timeout - MQTT will retry when WiFi connects");
        // MQTT will be initialized later when WiFi connects
    }

    // Main loop - handle MQTT commands and monitor system
    uint32_t counter = 0;

    while (1) {
        // Check for incoming commands from cloud
        Command_t cmd;
        if (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
            switch (cmd) {
                case CMD_START_EMER:
                    ESP_LOGI(TAG, ">>> Received START_EMER command!");
                    if (fsmEventQueue != NULL) {
                        FSMEvent_t event = {
                            .type = EVENT_CO_ALARM,
                            .co_ppm = 999.0f  // Force emergency
                        };
                        xQueueSend(fsmEventQueue, &event, 0);
                    }
                    break;
                case CMD_STOP_EMER:
                    ESP_LOGI(TAG, ">>> Received STOP_EMER command!");
                    if (fsmEventQueue != NULL) {
                        FSMEvent_t event = {
                            .type = EVENT_CMD_STOP_EMER,
                            .co_ppm = 0.0f
                        };
                        xQueueSend(fsmEventQueue, &event, 0);
                    }
                    break;
                case CMD_TEST:
                    ESP_LOGI(TAG, ">>> Received TEST command!");
                    // Trigger test alarm (short beep)
                    buzzer_set_active(true);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    buzzer_set_active(false);
                    break;
                case CMD_OPEN_DOOR:
                    ESP_LOGI(TAG, ">>> Received OPEN_DOOR command!");
                    door_open_request();
                    break;
                default:
                    break;
            }
        }

        // Status log
        SystemState_t current_state = fsm_get_state();
        const char *state_names[] = {"INIT", "NORMAL", "OPEN", "EMERGENCY"};
        ESP_LOGI(TAG, "WiFi: %s | MQTT: %s | Buffer: %d | State: %s | Count: %lu",
                 wifi_is_connected() ? "OK" : "NO",
                 mqtt_is_connected() ? "OK" : "NO",
                 ring_buffer_count(),
                 state_names[current_state],
                 (unsigned long)counter++);

        vTaskDelay(pdMS_TO_TICKS(STATUS_LOG_INTERVAL_MS));
    }
}
#endif // UNIT_TEST
