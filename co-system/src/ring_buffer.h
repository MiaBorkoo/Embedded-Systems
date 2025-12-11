#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

// Telemetry structure to store in buffer (for offline data)
typedef struct {
    uint32_t timestamp;
    float co_ppm;
    bool alarm_active;
    bool door_open;
    char event[16];
} RingBufferItem_t;

// Buffer size - store up to 100 readings when offline
#define RING_BUFFER_SIZE 100

// API
void ring_buffer_init(void);
bool ring_buffer_push(RingBufferItem_t *item);   // Add item, overwrite oldest if full
bool ring_buffer_pop(RingBufferItem_t *item);    // Remove oldest item
bool ring_buffer_is_empty(void);
int  ring_buffer_count(void);
void ring_buffer_clear(void);

#endif // RING_BUFFER_H
