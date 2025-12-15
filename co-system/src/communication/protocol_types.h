#ifndef PROTOCOL_TYPES_H
#define PROTOCOL_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/**
 * System status for STATUS packet
 */
typedef struct {
    bool armed;    // true if system is armed
    uint8_t state; // current system state
} Status_t;

#endif // PROTOCOL_TYPES_H