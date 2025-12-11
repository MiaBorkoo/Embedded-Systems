#include "sensor.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CO_Sensor";

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
        int adc_reading = adc1_get_raw(ADC1_CHANNEL_6);
        ESP_LOGI(TAG, "CO Sensor ADC: %d", adc_reading);

        vTaskDelay(pdMS_TO_TICKS(500)); // read every 0.5 sec
    }
}