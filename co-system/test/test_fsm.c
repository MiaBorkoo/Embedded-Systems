#include <unity.h>
#include "fsm/fsm.h"
#include "door_task/door.h"
#include "buzzer_task/buzzer.h"
#include "emergency_state/emergency.h"
#include "communication/agent_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "FSM_TEST";

// Test: Initial state is NORMAL
void test_initial_state(void) {
    TEST_ASSERT_EQUAL(STATE_NORMAL, fsm_get_state());
}

// Test: Button press transitions NORMAL -> OPEN -> NORMAL
void test_button_open_close(void) {
    FSMEvent_t event = {.type = EVENT_BUTTON_PRESS, .co_ppm = 0.0f};
    xQueueSend(fsmEventQueue, &event, 0);
    vTaskDelay(pdMS_TO_TICKS(100)); // Let FSM process
    TEST_ASSERT_EQUAL(STATE_OPEN, fsm_get_state());
    
    vTaskDelay(pdMS_TO_TICKS(5100)); // Wait for auto-close
    TEST_ASSERT_EQUAL(STATE_NORMAL, fsm_get_state());
}

// Test: CO alarm triggers EMERGENCY
void test_co_alarm_emergency(void) {
    FSMEvent_t event = {.type = EVENT_CO_ALARM, .co_ppm = 50.0f};
    xQueueSend(fsmEventQueue, &event, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(STATE_EMERGENCY, fsm_get_state());
}

// Test: CMD_RESET exits EMERGENCY
void test_reset_from_emergency(void) {
    // First trigger emergency with realistic CO alarm
    FSMEvent_t alarm = {.type = EVENT_CO_ALARM, .co_ppm = 50.0f};
    xQueueSend(fsmEventQueue, &alarm, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(STATE_EMERGENCY, fsm_get_state());
    
    // Then reset
    FSMEvent_t reset = {.type = EVENT_CMD_RESET, .co_ppm = 0.0f};
    xQueueSend(fsmEventQueue, &reset, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(STATE_NORMAL, fsm_get_state());
}

// Test: Button ignored in EMERGENCY state
void test_button_ignored_in_emergency(void) {
    // Trigger emergency with realistic CO alarm
    FSMEvent_t alarm = {.type = EVENT_CO_ALARM, .co_ppm = 50.0f};
    xQueueSend(fsmEventQueue, &alarm, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(STATE_EMERGENCY, fsm_get_state());
    
    // Try button press - should stay in EMERGENCY
    FSMEvent_t button = {.type = EVENT_BUTTON_PRESS, .co_ppm = 0.0f};
    xQueueSend(fsmEventQueue, &button, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(STATE_EMERGENCY, fsm_get_state());
    
    // Cleanup
    FSMEvent_t reset = {.type = EVENT_CMD_RESET, .co_ppm = 0.0f};
    xQueueSend(fsmEventQueue, &reset, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void setUp(void) {}
void tearDown(void) {}

void app_main(void) {
    ESP_LOGI(TAG, "FSM Unit Tests Starting...");
    
    // Initialize required components (no WiFi/MQTT for tests)
    agent_task_init();     // Creates telemetryQueue
    door_init();
    buzzer_init();
    emergency_init();
    fsm_init();
    
    // Give FSM time to initialize
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Running tests...");
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_button_open_close);
    RUN_TEST(test_co_alarm_emergency);
    RUN_TEST(test_reset_from_emergency);
    RUN_TEST(test_button_ignored_in_emergency);
    UNITY_END();
    
    ESP_LOGI(TAG, "Tests complete!");
    
    // Keep task alive
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

