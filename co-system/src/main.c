#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "wifi_manager.h"
#include "mqtt_handler.h"
#include "shared_types.h"

static const char *TAG = "MAIN";

// Queue for commands from MQTT
QueueHandle_t commandQueue = NULL;

void app_main(void)
{
    ESP_LOGI(TAG, "CO Safety System starting...");

    // Create command queue
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

    // Wait for MQTT to connect
    ESP_LOGI(TAG, "Waiting for MQTT connection...");
    int mqtt_wait = 0;
    while (!mqtt_is_connected() && mqtt_wait < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        mqtt_wait++;
    }

    if (mqtt_is_connected()) {
        ESP_LOGI(TAG, "MQTT connected!");
        // Publish initial status
        mqtt_publish_status("SAFE");
    } else {
        ESP_LOGW(TAG, "MQTT connection timeout, continuing anyway...");
    }

    // Test variables
    float test_co = 10.0;
    uint32_t counter = 0;

    // Main loop
    while (1) {
        // Check for incoming commands
        Command_t cmd;
        if (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
            switch (cmd) {
                case CMD_OPEN_DOOR:
                    ESP_LOGI(TAG, ">>> Received OPEN_DOOR command!");
                    break;
                case CMD_RESET:
                    ESP_LOGI(TAG, ">>> Received RESET command!");
                    break;
                case CMD_TEST:
                    ESP_LOGI(TAG, ">>> Received TEST command!");
                    break;
                default:
                    break;
            }
        }

        // Publish test CO reading every 2 seconds
        if (mqtt_is_connected()) {
            uint32_t timestamp = (uint32_t)(esp_timer_get_time() / 1000);
            mqtt_publish_co(test_co, timestamp);
            ESP_LOGI(TAG, "Published CO: %.2f ppm", test_co);

            // Simulate varying CO levels
            test_co += 0.5;
            if (test_co > 50.0) test_co = 10.0;
        }

        // Print status
        ESP_LOGI(TAG, "WiFi: %s | MQTT: %s | Count: %lu",
                 wifi_is_connected() ? "OK" : "NO",
                 mqtt_is_connected() ? "OK" : "NO",
                 (unsigned long)counter++);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

//RUAN - THIS IS THE HARDWARE STUFF
//door_init();  //initalises door system its a gpio interrupt , dont need to poll
//its the green led, servo motor and button 

    // emergency_init(); // emergency rn is just red LED, open door
    // emergency_trigger(true); // for the emergency state to work, door needs to be initialised first

    // buzzer_init();
    // buzzer_set_active(true); - this make the buzzer beep continously

    //sensor_init(); this is the CO sensor 