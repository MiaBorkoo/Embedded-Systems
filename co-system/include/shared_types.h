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

#endif // SHARED_TYPES_H
