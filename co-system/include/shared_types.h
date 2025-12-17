#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

/**
 * Command types received from cloud via MQTT
 * Parsed in mqtt_handler.c, handled in main.c
 */
typedef enum {
    CMD_NONE = 0,       // No command / invalid
    CMD_START_EMER,     // Start emergency mode
    CMD_STOP_EMER,      // Stop emergency mode
    CMD_TEST,           // Test alarm
    CMD_OPEN_DOOR       // Request door open for ventilation
} Command_t;

/**
 * System states - managed by FSM
 */
typedef enum {
    STATE_INIT = 0,        // Initialization/self-test (3 seconds)
    STATE_NORMAL = 1,      // Normal operation, door closed
    STATE_OPEN = 2,        // Door open (button press)
    STATE_EMERGENCY = 3    // Emergency mode (CO alarm)
} SystemState_t;

/**
 * FSM event types
 */
typedef enum {
    EVENT_BUTTON_PRESS,
    EVENT_CO_ALARM,      // CO >= threshold
    EVENT_CMD_STOP_EMER  // Stop emergency from server
} EventType_t;

#endif // SHARED_TYPES_H
