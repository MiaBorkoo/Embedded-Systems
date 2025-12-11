#pragma once
#include <stdbool.h>

#define BUZZER_PIN 14
#define BUZZER_CHANNEL LEDC_CHANNEL_1
#define BUZZER_TIMER LEDC_TIMER_1
#define BUZZER_FREQ 1000   // 1 kHz tone

void buzzer_init(void);
void buzzer_set_active(bool active);