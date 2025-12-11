#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Door events
typedef enum {
    EVENT_NONE = 0,
    EVENT_MANUAL_OPEN,
    EVENT_EMERGENCY_OPEN,
    EVENT_CLOSED
} DoorEvent_t;

// Commands from cloud
typedef enum {
    CMD_NONE = 0,
    CMD_OPEN_DOOR,
    CMD_RESET,
    CMD_TEST
} Command_t;

// System states
typedef enum {
    STATE_SAFE,
    STATE_ALARM,
    STATE_COOLDOWN,
    STATE_DISARMED
} SystemState_t;

// Telemetry data passed between tasks
typedef struct {
    float co_level;
    SystemState_t state;
    bool state_changed;
    DoorEvent_t door_event;
    uint32_t timestamp;
} Telemetry_t;

#endif // SHARED_TYPES_H
