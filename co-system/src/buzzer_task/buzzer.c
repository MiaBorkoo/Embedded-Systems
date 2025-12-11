#include "buzzer.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "Buzzer";
static volatile bool buzzer_active = false;

#define BEEP_INTERVAL_MS 300

static void play_tone(bool on) {
    if (on) {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL, 2000);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL);
    } else {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL);
    }
}

void buzzer_set_active(bool active) {
    buzzer_active = active;
    if (!active) {
        play_tone(false);
    }
}

static void buzzer_task(void *arg) {
    bool tone_on = false;
    TickType_t lastToggle = xTaskGetTickCount();

    while (1) {
        if (buzzer_active) {
            TickType_t now = xTaskGetTickCount();
            if ((now - lastToggle) * portTICK_PERIOD_MS >= BEEP_INTERVAL_MS) {
                lastToggle = now;
                tone_on = !tone_on;
                play_tone(tone_on);
            }
        } else {
            play_tone(false);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void buzzer_init(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = BUZZER_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = BUZZER_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = BUZZER_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);

    xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Passive buzzer initialized");
}