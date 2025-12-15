#ifndef DOOR_H
#define DOOR_H
#include <stdbool.h>
#include "driver/gpio.h"

#define SERVO_PIN     25
#define GREEN_LED_PIN 26
#define RED_LED_PIN   27
#define BUTTON_PIN    12

void door_init(void);
void door_open_request(void);  
void door_set_angle(uint32_t angle);

#endif // DOOR_H