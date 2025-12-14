#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

/**
 * Command types received from cloud via MQTT
 * Parsed in mqtt_handler.c, handled in main.c
 */
typedef enum {
    CMD_NONE = 0,       // No command / invalid
    CMD_ARM,            // Arm system - enable CO monitoring and alarms
    CMD_DISARM,         // Disarm system - disable alarms (maintenance mode)
    CMD_TEST,           // Trigger test alarm sequence
    CMD_RESET,          // Reset alarm state after CO event cleared
    CMD_OPEN_DOOR       // Request door open for ventilation
} Command_t;

/**
 * System states - managed by FSM
 */
typedef enum {
    STATE_NORMAL = 0,      // Normal operation, door closed
    STATE_OPEN = 1,        // Door open (button press)
    STATE_EMERGENCY = 2    // Emergency mode (CO alarm)
} SystemState_t;

/**
 * FSM event types
 */
typedef enum {
    EVENT_BUTTON_PRESS,
    EVENT_CO_ALARM,      // CO >= threshold
    EVENT_CMD_TEST,      // Test alarm from server
    EVENT_CMD_RESET      // Reset emergency from server
} EventType_t;

#endif // SHARED_TYPES_H
