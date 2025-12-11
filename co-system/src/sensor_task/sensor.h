#ifndef SENSOR_H
#define SENSOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CO_SENSOR_PIN 34  //ESP32 ADC1 channel 6

void sensor_init(void);
void sensor_task(void *arg);

#endif