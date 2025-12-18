#pragma once
#include <stdbool.h>
#include "config.h"

#define BUZZER_CHANNEL LEDC_CHANNEL_1
#define BUZZER_TIMER LEDC_TIMER_1

void buzzer_init(void);
void buzzer_set_active(bool active);