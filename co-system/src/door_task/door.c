#include "door.h"
#include "fsm/fsm.h"
#include "config.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "DoorTask";

// Servo PWM parameters
#define SERVO_FREQ    SERVO_FREQ_HZ
#define SERVO_MIN_US  SERVO_MIN_PULSE_US
#define SERVO_MAX_US  SERVO_MAX_PULSE_US
#define SERVO_TIMER   LEDC_TIMER_0
#define SERVO_CHANNEL LEDC_CHANNEL_0

static volatile int64_t last_press_time = 0;

// Convert angle to duty
static uint32_t angle_to_duty(uint32_t angle) {
    uint32_t us = SERVO_MIN_US + ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180;
    return (us * 65535) / (1000000 / SERVO_FREQ);
}

// Servo control
void door_set_angle(uint32_t angle) {
    uint32_t duty = angle_to_duty(angle);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, SERVO_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, SERVO_CHANNEL);
}

// ISR for button - send event to FSM
void IRAM_ATTR button_isr_handler(void* arg) {
    int64_t now = esp_timer_get_time();
    if (now - last_press_time > BUTTON_DEBOUNCE_MS * 1000) {
        last_press_time = now;
        
        // Send button press event to FSM
        if (fsmEventQueue != NULL) {
            FSMEvent_t event = {
                .type = EVENT_BUTTON_PRESS,
                .co_ppm = 0.0f
            };
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(fsmEventQueue, &event, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

// Optional to trigger manually from MQTT command
void door_open_request(void) {
    if (fsmEventQueue != NULL) {
        FSMEvent_t event = {
            .type = EVENT_BUTTON_PRESS,
            .co_ppm = 0.0f
        };
        xQueueSend(fsmEventQueue, &event, 0);
    }
}

void door_init(void) {
    // Servo setup
    ledc_timer_config_t servo_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = SERVO_TIMER,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .freq_hz = SERVO_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&servo_timer);

    ledc_channel_config_t servo_channel = {
        .gpio_num = SERVO_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = SERVO_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_TIMER,
        .duty = angle_to_duty(0),
        .hpoint = 0
    };
    ledc_channel_config(&servo_channel);

    //button
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&btn_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    ESP_LOGI(TAG, "Door system initialized");
}