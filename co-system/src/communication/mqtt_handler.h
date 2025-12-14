#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

void mqtt_init(void);
bool mqtt_is_connected(void);

// Publishing functions
bool mqtt_publish_telemetry(float co_ppm, uint32_t timestamp, uint8_t state, bool alarm_active, bool door_open);
bool mqtt_publish_event(const char *event_type, float co_ppm, uint32_t timestamp);
bool mqtt_publish_status(uint8_t state, bool armed);

// For teammate's binary protocol later
bool mqtt_publish_raw(const char *topic, const uint8_t *data, size_t len, int qos);

#endif
