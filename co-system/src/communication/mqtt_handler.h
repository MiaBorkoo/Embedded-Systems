#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "config.h"


#define TOPIC_CO        MQTT_TOPIC_CO
#define TOPIC_DOOR      MQTT_TOPIC_EVENTS
#define TOPIC_STATUS    MQTT_TOPIC_STATUS
#define TOPIC_COMMANDS  MQTT_TOPIC_COMMANDS


void mqtt_init(void);
bool mqtt_is_connected(void);

bool mqtt_publish_raw(const char *topic, const uint8_t *data, size_t len, int qos);

#endif
