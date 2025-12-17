// stats.h
#pragma once
#include "shared_types.h"
#include <stdint.h>

#define CO_HISTORY_SIZE 10   // Last N CO readings
#define STATS_PERIOD_MS 5000 // Print stats every 5 seconds

typedef struct {
    uint32_t telemetry_sent;
    uint32_t telemetry_buffered;
    uint32_t events_sent;    

    // CO tracking
    uint32_t co_reads;                  
    float co_history[CO_HISTORY_SIZE];  // circular buffer of last CO readings
    uint8_t co_index;  
} SystemStats_t;

// API
void stats_init(void);
void stats_task_init(void); 
void stats_record_co(float co_ppm);
void stats_record_telemetry_sent(void);
void stats_record_telemetry_buffered(void);
void stats_record_event_sent(void);