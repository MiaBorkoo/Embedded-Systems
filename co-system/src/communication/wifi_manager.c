#include "wifi_manager.h"
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI";
// Event group to signal WiFi connection status
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static TimerHandle_t reconnect_timer = NULL;
static bool reconnect_pending = false;
static int init_retry_count = 0;
static bool init_phase = true;

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
        init_retry_count = 0;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        
        // During init phase, retry up to 3 times quickly
        if (init_phase && init_retry_count < WIFI_MAX_RETRY) {
            init_retry_count++;
            ESP_LOGW(TAG, "WiFi connection failed (attempt %d/%d), retrying...", 
                     init_retry_count, WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            // After init phase or max retries, switch to 5-second background retries
            if (init_phase) {
                ESP_LOGW(TAG, "Initial connection attempts exhausted, switching to 5s retry cycle");
                init_phase = false;
            } else {
                ESP_LOGW(TAG, "WiFi disconnected, will retry in 5 seconds...");
            }
            
            // Schedule reconnect after 5 seconds
            if (!reconnect_pending) {
                reconnect_pending = true;
                if (reconnect_timer != NULL) {
                    xTimerStart(reconnect_timer, 0);
                }
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        reconnect_pending = false;
        init_phase = false;  // Successfully connected, exit init phase
        
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
        pdMS_TO_TICKS(WIFI_RECONNECT_MS),
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
            .threshold.authmode = WIFI_AUTH_OPEN,  
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start WiFi (non-blocking - connection happens in background)
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization complete (connecting in background)");
}

bool wifi_is_connected(void)
{
    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
