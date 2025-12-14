#include "door_task/door.h"      // for RED_LED_PIN
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "Emergency";

// Initialize red LED (FSM now controls it directly)
void emergency_init(void) {
    // Red LED setup
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RED_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(RED_LED_PIN, 0);

    ESP_LOGI(TAG, "Emergency system initialized (Red LED only)");
}