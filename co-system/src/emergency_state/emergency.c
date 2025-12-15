#include "door_task/door.h"
#include "esp_log.h"

static const char *TAG = "Emergency";

// No-op: LED initialization is now handled centrally by FSM
void emergency_init(void) {
    ESP_LOGI(TAG, "Emergency module ready (LEDs controlled by FSM)");
}