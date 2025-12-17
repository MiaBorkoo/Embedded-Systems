#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "config.h"

// MQTT Topics defined in config.h:
// MQTT_TOPIC_CO, MQTT_TOPIC_EVENTS, MQTT_TOPIC_STATUS, MQTT_TOPIC_COMMANDS
// Legacy names for backward compatibility:
#define TOPIC_CO        MQTT_TOPIC_CO
#define TOPIC_DOOR      MQTT_TOPIC_EVENTS
#define TOPIC_STATUS    MQTT_TOPIC_STATUS
#define TOPIC_COMMANDS  MQTT_TOPIC_COMMANDS


void mqtt_init(void);
bool mqtt_is_connected(void);

// Publish raw binary data (used by agent_task with protocol encoder)
bool mqtt_publish_raw(const char *topic, const uint8_t *data, size_t len, int qos);

#endif
