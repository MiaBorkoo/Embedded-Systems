#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

// Initialize MQTT client and connect to broker
void mqtt_init(void);

// Check if connected to MQTT broker
bool mqtt_is_connected(void);

// Publish CO sensor reading
void mqtt_publish_co(float co_level, uint32_t timestamp);

// Publish door event
// event_type: "MANUAL_OPEN", "EMERGENCY_OPEN", "CLOSED"
void mqtt_publish_door_event(const char *event_type, float co_level, uint32_t timestamp);

// Publish system status (retained message)
// state: "SAFE", "ALARM", "COOLDOWN", "OFFLINE"
void mqtt_publish_status(const char *state);

#endif // MQTT_HANDLER_H
