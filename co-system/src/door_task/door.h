#ifndef DOOR_H
#define DOOR_H
#include <stdbool.h>
#include "driver/gpio.h"
#include "config.h"

// Pin definitions from config.h:
// SERVO_PIN, GREEN_LED_PIN, RED_LED_PIN, BUTTON_PIN

void door_init(void);
void door_open_request(void);  
void door_set_angle(uint32_t angle);

#endif // DOOR_H