#include "door.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static const char *TAG = "DoorTask";

// Servo PWM parameters
#define SERVO_FREQ    50
#define SERVO_MIN_US  500
#define SERVO_MAX_US  2400
#define SERVO_TIMER   LEDC_TIMER_0
#define SERVO_CHANNEL LEDC_CHANNEL_0

#define DOOR_OPEN_MS    5000
#define DEBOUNCE_MS     200   // 200ms debounce

// Semaphore to signal door open request
static SemaphoreHandle_t door_sem;
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

// ISR for button
void IRAM_ATTR button_isr_handler(void* arg) {
    int64_t now = esp_timer_get_time();
    if (now - last_press_time > DEBOUNCE_MS * 1000) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(door_sem, &xHigherPriorityTaskWoken);
        last_press_time = now;
    }
}

// Optional to trigger manually
void door_open_request(void) {
    xSemaphoreGive(door_sem);
}

// Main door task
static void door_task(void *arg) {
    while (1) {
        if (xSemaphoreTake(door_sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Opening door");
            door_set_angle(90);

            vTaskDelay(pdMS_TO_TICKS(DOOR_OPEN_MS));

            ESP_LOGI(TAG, "Closing door");
            door_set_angle(0);
        }
    }
}

void door_init(void) {

    // GREEN LED ONLY — always ON
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GREEN_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    gpio_set_level(GREEN_LED_PIN, 1);   // green LED is always ON

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

    door_sem = xSemaphoreCreateBinary();

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    xTaskCreate(door_task, "door_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Door system initialized (Green LED always on)");
}


//RUAN - THIS IS THE HARDWARE STUFF
//door_init();  //initalises door system its a gpio interrupt , dont need to poll
//its the green led, servo motor and button 

    // emergency_init(); // emergency rn is just red LED, open door
    // emergency_trigger(true); // for the emergency state to work, door needs to be initialised first

    // buzzer_init();
    // buzzer_set_active(true); - this make the buzzer beep continously

    //sensor_init(); this is the CO sensor 
