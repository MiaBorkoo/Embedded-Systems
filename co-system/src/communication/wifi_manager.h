#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

// Initialize WiFi in station mode and start connection
void wifi_init(void);

// Check if WiFi is currently connected
bool wifi_is_connected(void);

#endif
