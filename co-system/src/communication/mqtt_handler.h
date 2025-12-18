#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "config.h"

void mqtt_init(void);
bool mqtt_is_connected(void);

bool mqtt_publish_raw(const char *topic, const uint8_t *data, size_t len, int qos);

#endif
