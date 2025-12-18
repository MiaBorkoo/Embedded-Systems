#ifndef FSM_H
#define FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "shared_types.h"
#include "config.h"

// External queue for sending events to FSM
extern QueueHandle_t fsmEventQueue;

// Initialize and start FSM
void fsm_init(void);

// Get current system state (thread-safe)
SystemState_t fsm_get_state(void);

#endif // FSM_H

