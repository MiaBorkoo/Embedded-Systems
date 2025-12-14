#ifndef FSM_H
#define FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "shared_types.h"

// Event structure
typedef struct {
    EventType_t type;
    float co_ppm;        // Used for CO_ALARM events
} FSMEvent_t;

// FSM task priority (between agent=1 and sensor=10)
#define FSM_TASK_PRIORITY  5
#define FSM_TASK_STACK     4096

// External queue for sending events to FSM
extern QueueHandle_t fsmEventQueue;

// Initialize and start FSM
void fsm_init(void);

// Get current system state (thread-safe)
SystemState_t fsm_get_state(void);

#endif // FSM_H

