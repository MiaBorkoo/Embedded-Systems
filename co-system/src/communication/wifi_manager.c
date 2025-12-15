#include "wifi_manager.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#define WIFI_SSID     "Ruan's A15"
#define WIFI_PASSWORD "odowd_A15!!!"
#define WIFI_INIT_MAX_RETRY     3
#define WIFI_INIT_TIMEOUT_MS    10000
#define WIFI_RECONNECT_DELAY_MS 5000

static const char *TAG = "WIFI";
// Event group to signal WiFi connection status
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static TimerHandle_t reconnect_timer = NULL;
static bool reconnect_pending = false;

//All of this is based of of https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html

// Timer callback to retry WiFi connection
static void reconnect_timer_callback(TimerHandle_t xTimer)
{
    if (!wifi_is_connected() && reconnect_pending) {
        ESP_LOGI(TAG, "Attempting WiFi reconnection...");
        reconnect_pending = false;
        esp_wifi_connect();
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to SSID: %s", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, will retry in 5 seconds...");
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        
        // Schedule reconnect after 5 seconds
        if (!reconnect_pending) {
            reconnect_pending = true;
            if (reconnect_timer != NULL) {
                xTimerStart(reconnect_timer, 0);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        reconnect_pending = false;
        
        // Stop timer if running
        if (reconnect_timer != NULL) {
            xTimerStop(reconnect_timer, 0);
        }
    }
}

void wifi_init(void)
{
    ESP_LOGI(TAG, "WiFi initializing...");

    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create event group
    wifi_event_group = xEventGroupCreate();

    // Create reconnect timer (one-shot timer)
    reconnect_timer = xTimerCreate(
        "wifi_reconnect",
        pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS),
        pdFALSE,  // One-shot timer
        NULL,
        reconnect_timer_callback
    );

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Try to connect up to 3 times on init
    bool connected = false;
    for (int retry = 0; retry < WIFI_INIT_MAX_RETRY && !connected; retry++) {
        ESP_LOGI(TAG, "WiFi connection attempt %d/%d", retry + 1, WIFI_INIT_MAX_RETRY);
        
        EventBits_t bits = xEventGroupWaitBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(WIFI_INIT_TIMEOUT_MS)
        );

        if (bits & WIFI_CONNECTED_BIT) {
            connected = true;
            ESP_LOGI(TAG, "WiFi connected successfully on attempt %d", retry + 1);
        } else if (retry < WIFI_INIT_MAX_RETRY - 1) {
            ESP_LOGW(TAG, "Connection timeout, retrying...");
        }
    }

    if (!connected) {
        ESP_LOGW(TAG, "Failed to connect after %d attempts, will retry in background", WIFI_INIT_MAX_RETRY);
    }

    ESP_LOGI(TAG, "WiFi initialization complete");
}

bool wifi_is_connected(void)
{
    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
