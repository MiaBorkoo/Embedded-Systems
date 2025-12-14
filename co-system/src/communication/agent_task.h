#ifndef AGENT_TASK_H
#define AGENT_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Agent task priority - lowest (cloud comms not safety-critical)
#define AGENT_TASK_PRIORITY  1
#define AGENT_TASK_STACK     4096

// Initialize and start the agent task
void agent_task_init(void);

// The agent task function (FreeRTOS task)
void agent_task(void *params);

// External queue - receives Telemetry_t from alarm_task
extern QueueHandle_t telemetryQueue;

#endif // AGENT_TASK_H
