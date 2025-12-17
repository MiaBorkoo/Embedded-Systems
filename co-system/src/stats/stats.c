#include "stats.h"
#include "fsm/fsm.h"
#include "../communication/agent_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "STATS";
static SystemStats_t stats;

void stats_init(void) {
    memset(&stats, 0, sizeof(stats));
}

void stats_record_co(float co_ppm) {
    stats.co_history[stats.co_index] = co_ppm;
    stats.co_index = (stats.co_index + 1) % CO_HISTORY_SIZE;
    stats.co_reads++;
}

void stats_record_telemetry_sent(void) {
    stats.telemetry_sent++;
}

void stats_record_telemetry_buffered(void) {
    stats.telemetry_buffered++;
}

void stats_record_event_sent(void) {
    stats.events_sent++;
}

// Periodic task to print statistics
void stats_task(void *arg) {
    while (1) {
        // Get current FSM state
        SystemState_t current_state = fsm_get_state();

        // Safe array of state names including INIT
        const char *state_names[] = {"INIT", "NORMAL", "OPEN", "EMERGENCY"};
        size_t state_count = sizeof(state_names) / sizeof(state_names[0]);

        // Clamp state to valid range
        if ((size_t)current_state >= state_count) {
            ESP_LOGW(TAG, "Unknown FSM state: %d", current_state);
            current_state = 1; // default to NORMAL
        }

        const char *current_state_name = state_names[current_state];

        // Print summary
        ESP_LOGI(TAG, "---- SYSTEM STATS ----");
        ESP_LOGI(TAG, "Telemetry sent: %u | Buffered: %u | Events sent: %u",
                stats.telemetry_sent, stats.telemetry_buffered, stats.events_sent);
        ESP_LOGI(TAG, "Current FSM state: %s", current_state_name);

        // Print last 10 CO readings
        char buf[128] = "";
        for (int i = 0; i < CO_HISTORY_SIZE; i++) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%.1f ", stats.co_history[i]);
            strcat(buf, tmp);
        }
        ESP_LOGI(TAG, "Last %d CO readings: %s", CO_HISTORY_SIZE, buf);
        ESP_LOGI(TAG, "---------------------");

        vTaskDelay(pdMS_TO_TICKS(STATS_PERIOD_MS));
    }
}

void stats_task_init(void) {
    stats_init();
    xTaskCreate(stats_task, "stats_task", 4096, NULL, 1, NULL);
}