#pragma once
#include <stdbool.h>
#include "config.h"

// Pin and configuration from config.h:
// PIN_BUZZER, BUZZER_FREQ_HZ

#define BUZZER_CHANNEL LEDC_CHANNEL_1
#define BUZZER_TIMER LEDC_TIMER_1

void buzzer_init(void);
void buzzer_set_active(bool active);