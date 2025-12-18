#ifndef DOOR_H
#define DOOR_H
#include <stdbool.h>
#include "driver/gpio.h"
#include "config.h"

void door_init(void);
void door_open_request(void);  
void door_set_angle(uint32_t angle);

#endif // DOOR_H