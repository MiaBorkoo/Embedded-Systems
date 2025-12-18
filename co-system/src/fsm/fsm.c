#include "fsm.h"
#include "door_task/door.h"
#include "buzzer_task/buzzer.h"
#include "emergency_state/emergency.h"
#include "communication/agent_task.h"
#include "communication/ring_buffer.h"
#include "communication/ifttt_webhook.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include <string.h>
#include "stats/stats.h"

static const char *TAG = "FSM";

// Global CO reading from sensor task (defined in sensor.c)
extern volatile float g_current_co_ppm;

// Event queue
QueueHandle_t fsmEventQueue = NULL;

// Current state (protected by mutex)
static SystemState_t current_state = STATE_INIT;
static SemaphoreHandle_t state_mutex = NULL;

// Emergency webhook trigger flag (prevents duplicate triggers)
static bool emergency_webhook_triggered = false;

// Get current state (thread-safe)
SystemState_t fsm_get_state(void) {
    // Handle case where FSM not yet initialized
    if (state_mutex == NULL) {
        return STATE_NORMAL;  // Return default state if FSM not ready
    }
    
    SystemState_t state;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state = current_state;
    xSemaphoreGive(state_mutex);
    return state;
}

// Set current state (thread-safe)
static void fsm_set_state(SystemState_t new_state) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    current_state = new_state;
    xSemaphoreGive(state_mutex);

}

// Send telemetry event to cloud
static void send_telemetry_event(const char *event, float co_ppm, SystemState_t state) {
    if (telemetryQueue == NULL) return;
    
    Telemetry_t telem = {
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000),
        .co_ppm = co_ppm,
        .alarm_active = (state == STATE_EMERGENCY),
        .door_open = (state == STATE_OPEN || state == STATE_EMERGENCY),
        .state = state,
    };
    strncpy(telem.event, event, sizeof(telem.event) - 1);
    telem.event[sizeof(telem.event) - 1] = '\0';
    
    xQueueSend(telemetryQueue, &telem, 0);
}

// Apply hardware configuration for current state
static void apply_state_config(SystemState_t state, float co_ppm) {
    switch (state) {
        case STATE_INIT:
            ESP_LOGI(TAG, ">>> STATE: INIT (self-test for 3s)");
            gpio_set_level(GREEN_LED_PIN, 1);  // Green ON
            gpio_set_level(RED_LED_PIN, 1);    // Red ON
            door_set_angle(90);                 // Door open
            buzzer_set_active(true);            // Buzzer ON
            // send_telemetry_event("STATE_INIT", co_ppm, STATE_INIT);
            break;
            
        case STATE_NORMAL:
            ESP_LOGI(TAG, ">>> STATE: NORMAL");
            gpio_set_level(GREEN_LED_PIN, 1);  // Green ON
            gpio_set_level(RED_LED_PIN, 0);    // Red OFF
            door_set_angle(0);                  // Door closed
            buzzer_set_active(false);           // Buzzer OFF
            send_telemetry_event("STATE_NORMAL", co_ppm, STATE_NORMAL);
            // Reset webhook flag when returning to normal
            emergency_webhook_triggered = false;
            break;
            
        case STATE_OPEN:
            ESP_LOGI(TAG, ">>> STATE: OPEN (ventilation mode)");
            gpio_set_level(GREEN_LED_PIN, 1);  // Green ON
            gpio_set_level(RED_LED_PIN, 0);    // Red OFF
            door_set_angle(90);                 // Door open
            buzzer_set_active(false);           // Buzzer OFF
            send_telemetry_event("STATE_OPEN", co_ppm, STATE_OPEN);
            break;
            
        case STATE_EMERGENCY:
            ESP_LOGI(TAG, ">>> STATE: EMERGENCY (CO ALARM!)");
            gpio_set_level(GREEN_LED_PIN, 0);  // Green OFF
            gpio_set_level(RED_LED_PIN, 1);    // Red ON
            door_set_angle(90);                 // Door open for ventilation
            buzzer_set_active(true);            // Buzzer ON
            send_telemetry_event("EMERGENCY_ON", co_ppm, STATE_EMERGENCY);
            
            // Trigger IFTTT webhook once per emergency session
            if (!emergency_webhook_triggered) {
                ifttt_webhook_trigger();
                emergency_webhook_triggered = true;
            }
            break;
    }
}

// FSM state transition logic
static void handle_event(FSMEvent_t *event) {
    SystemState_t prev_state = fsm_get_state();
    SystemState_t next_state = prev_state;

    switch (prev_state) {
        case STATE_INIT:
            switch (event->type) {
                case EVENT_BUTTON_PRESS:
                    // Button ignored during initialization
                    ESP_LOGW(TAG, "Button press ignored in INIT state");
                    break;
                case EVENT_CO_ALARM:
                    // CO alarm immediately transitions to emergency
                    next_state = STATE_EMERGENCY;
                    break;
                case EVENT_CMD_STOP_EMER:
                    // Stop emergency command ignored during initialization
                    break;
            }
            break;

        case STATE_NORMAL:
            switch (event->type) {
                case EVENT_BUTTON_PRESS:
                    next_state = STATE_OPEN;
                    break;
                case EVENT_CO_ALARM:
                    stats_record_co(event->co_ppm); 
                    next_state = STATE_EMERGENCY;
                    break;
                case EVENT_CMD_STOP_EMER:
                    // Already in normal, ignore
                    break;
            }
            break;

        case STATE_OPEN:
            switch (event->type) {
                case EVENT_BUTTON_PRESS:
                    // Button allowed in OPEN, but already open - ignore
                    break;
                case EVENT_CO_ALARM:
                    stats_record_co(event->co_ppm);
                    next_state = STATE_EMERGENCY;
                    break;
                case EVENT_CMD_STOP_EMER:
                    // No emergency active, ignore
                    break;
            }
            break;

        case STATE_EMERGENCY:
            switch (event->type) {
                case EVENT_BUTTON_PRESS:
                    // Button ignored in EMERGENCY
                    ESP_LOGW(TAG, "Button press ignored in EMERGENCY state");
                    break;
                case EVENT_CO_ALARM:
                    stats_record_co(event->co_ppm);
                    // Already in emergency, update CO reading
                    ESP_LOGD(TAG, "Still in EMERGENCY (CO=%.1f ppm)", event->co_ppm);
                    break;
                case EVENT_CMD_STOP_EMER:
                    next_state = STATE_NORMAL;
                    break;
            }
            break;
    }

    // Apply state transition using current sensor CO reading
    if (next_state != prev_state) {
        fsm_set_state(next_state);
        apply_state_config(next_state, g_current_co_ppm);
    }
}

// FSM task - main loop
static void fsm_task(void *arg) {
    FSMEvent_t event;
    TickType_t init_start_time = 0;
    TickType_t door_close_time = 0;
    bool init_timer_active = false;
    bool door_timer_active = false;
    
    ESP_LOGI(TAG, "FSM task started (Priority %d)", TASK_PRIORITY_FSM);
    
    // Set initial state and start init timer
    apply_state_config(STATE_INIT, 0.0f);
    init_start_time = xTaskGetTickCount();
    init_timer_active = true;
    
    while (1) {
        // Check for events (50ms timeout)
        if (xQueueReceive(fsmEventQueue, &event, pdMS_TO_TICKS(50)) == pdTRUE) {
            handle_event(&event);
            
            // Start door timer if entering OPEN state
            if (fsm_get_state() == STATE_OPEN) {
                door_close_time = xTaskGetTickCount();
                door_timer_active = true;
            }
            
            // Cancel door timer if leaving OPEN state
            if (fsm_get_state() != STATE_OPEN) {
                door_timer_active = false;
            }
            
            // Cancel init timer if leaving INIT state (e.g., CO alarm)
            if (fsm_get_state() != STATE_INIT) {
                init_timer_active = false;
            }
        }
        
        // Handle init auto-transition timer
        if (init_timer_active && fsm_get_state() == STATE_INIT) {
            TickType_t elapsed = (xTaskGetTickCount() - init_start_time) * portTICK_PERIOD_MS;
            if (elapsed >= INIT_DURATION_MS) {
                ESP_LOGI(TAG, "Init timer expired, transitioning to NORMAL");
                fsm_set_state(STATE_NORMAL);
                apply_state_config(STATE_NORMAL, g_current_co_ppm);
                init_timer_active = false;
            }
        }

        // Handle door auto-close timer
        if (door_timer_active && fsm_get_state() == STATE_OPEN) {
            TickType_t elapsed = (xTaskGetTickCount() - door_close_time) * portTICK_PERIOD_MS;
            if (elapsed >= DOOR_OPEN_DURATION_MS) {
                ESP_LOGI(TAG, "Door timer expired, returning to NORMAL");
                fsm_set_state(STATE_NORMAL);
                apply_state_config(STATE_NORMAL, g_current_co_ppm);
                door_timer_active = false;
            }
        }
    }
}

void fsm_init(void) {
    // Initialize LED GPIOs (centralized control)
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << GREEN_LED_PIN) | (1ULL << RED_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    
    // Create state mutex
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return;
    }
    
    // Create event queue
    fsmEventQueue = xQueueCreate(QUEUE_SIZE_FSM_EVENT, sizeof(FSMEvent_t));
    if (fsmEventQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
    
    // Create FSM task
    BaseType_t ret = xTaskCreate(
        fsm_task,
        "fsm_task",
        TASK_STACK_FSM,
        NULL,
        TASK_PRIORITY_FSM,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create FSM task");
        return;
    }
    
    ESP_LOGI(TAG, "FSM initialized");
}

