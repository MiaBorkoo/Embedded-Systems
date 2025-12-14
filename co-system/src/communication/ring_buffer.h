#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

// Telemetry structure - what alarm_task sends us
typedef struct {
    uint32_t timestamp;
    float co_ppm;
    bool alarm_active;
    bool door_open;
    uint8_t state;          // 0=SAFE, 1=WARNING, 2=ALARM, 3=DISARMED
    char event[16];         // "READING", "ALARM_ON", "DOOR_OPEN", etc.
} Telemetry_t;

#define RING_BUFFER_SIZE 100

void ring_buffer_init(void);
bool ring_buffer_push(Telemetry_t *item);   // Returns false if mutex timeout
bool ring_buffer_pop(Telemetry_t *item);    // Returns false if empty
bool ring_buffer_is_empty(void);
int  ring_buffer_count(void);
void ring_buffer_clear(void);

#endif
