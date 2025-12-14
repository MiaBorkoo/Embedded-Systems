#include "sensor.h"
#include "fsm/fsm.h"
#include "communication/agent_task.h"
#include "communication/ring_buffer.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "CO_Sensor";

// CO threshold for triggering emergency
#define CO_THRESHOLD_PPM 35.0f

// Convert ADC reading to CO ppm
// Linear mapping: 0-4095 ADC -> 0-200 ppm CO
static float adc_to_co_ppm(int adc_value) {
    return (float)adc_value * 200.0f / 4095.0f;
}

void sensor_init(void)
{
    // ADC config
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // GPIO34 = ADC1_CH6

    // Create FreeRTOS task
    xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 10, NULL);
}

void sensor_task(void *arg)
{
    while (1)
    {
        // Read ADC
        int adc_reading = adc1_get_raw(ADC1_CHANNEL_6);
        float co_ppm = adc_to_co_ppm(adc_reading);
        
        ESP_LOGI(TAG, "CO Sensor ADC: %d -> %.1f ppm", adc_reading, co_ppm);
        
        // Send to FSM if CO level exceeds threshold
        if (co_ppm >= CO_THRESHOLD_PPM && fsmEventQueue != NULL) {
            FSMEvent_t event = {
                .type = EVENT_CO_ALARM,
                .co_ppm = co_ppm
            };
            xQueueSend(fsmEventQueue, &event, 0);
        }
        
        // Send telemetry to cloud (for monitoring)
        if (telemetryQueue != NULL) {
            SystemState_t state = fsm_get_state();
            Telemetry_t telem = {
                .timestamp = (uint32_t)(esp_timer_get_time() / 1000),
                .co_ppm = co_ppm,
                .alarm_active = (state == STATE_EMERGENCY),
                .door_open = (state == STATE_OPEN || state == STATE_EMERGENCY),
                .state = state,
            };
            strncpy(telem.event, "READING", sizeof(telem.event) - 1);
            xQueueSend(telemetryQueue, &telem, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // read every 0.5 sec
    }
}