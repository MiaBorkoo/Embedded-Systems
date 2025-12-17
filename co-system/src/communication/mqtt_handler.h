#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// MQTT Topics
#define TOPIC_CO        "nonfunctionals/sensors/co"
#define TOPIC_DOOR      "nonfunctionals/events/door"
#define TOPIC_STATUS    "nonfunctionals/status"
#define TOPIC_COMMANDS  "nonfunctionals/commands"


void mqtt_init(void);
bool mqtt_is_connected(void);

// Publish raw binary data (used by agent_task with protocol encoder)
bool mqtt_publish_raw(const char *topic, const uint8_t *data, size_t len, int qos);

#endif
