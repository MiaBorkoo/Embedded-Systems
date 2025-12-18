#ifndef SENSOR_H
#define SENSOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

void sensor_init(void);
void sensor_task(void *arg);

#endif