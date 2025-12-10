#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "CO Safety System starting...");

    // Initialize WiFi
    wifi_init();

    // Test loop - print connection status every 2 seconds
    while (1) {
        if (wifi_is_connected()) {
            ESP_LOGI(TAG, "WiFi status: CONNECTED");
        } else {
            ESP_LOGW(TAG, "WiFi status: DISCONNECTED");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
