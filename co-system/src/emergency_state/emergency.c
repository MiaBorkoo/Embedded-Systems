#include "door_task/door.h"      // for GREEN_LED_PIN and SERVO_PIN
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "Emergency";
static volatile bool emergency_active = false;

// Convert angle to duty (same as in door.c)
static uint32_t angle_to_duty(uint32_t angle) {
    const uint32_t SERVO_MIN_US = 500;
    const uint32_t SERVO_MAX_US = 2400;
    const uint32_t SERVO_FREQ   = 50;
    uint32_t us = SERVO_MIN_US + ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180;
    return (us * 65535) / (1000000 / SERVO_FREQ);
}

// Servo PWM channel 
#define SERVO_TIMER   LEDC_TIMER_0
#define SERVO_CHANNEL LEDC_CHANNEL_0

void emergency_trigger(bool active) {
    emergency_active = active;
}

// Emergency task
static void emergency_task(void *arg) {
    while (1) {
        if (emergency_active) {
    
            gpio_set_level(RED_LED_PIN, 1); // turns Red LED ON

            gpio_set_level(GREEN_LED_PIN, 0);// turns Green LED OFF

            uint32_t duty = angle_to_duty(90);// Servo at 90°
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, SERVO_CHANNEL, duty);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, SERVO_CHANNEL);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // small delay to not hog CPU
    }
}

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

    //creates emergency task
    xTaskCreate(emergency_task, "emergency_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Emergency system initialized");
}