#include "ifttt_webhook.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "wifi_manager.h"

static const char *TAG = "IFTTT_WEBHOOK";

static QueueHandle_t webhookQueue = NULL;

static TaskHandle_t webhookTaskHandle = NULL;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGW(TAG, "HTTP error");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "Connected to IFTTT");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "Response: %.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected");
            break;
        default:
            break;
    }
    return ESP_OK;
}


static void send_webhook_post(void)
{
    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, skipping webhook");
        return;
    }

    ESP_LOGI(TAG, "Sending emergency webhook to IFTTT...");

    esp_http_client_config_t config = {
        .url = IFTTT_WEBHOOK_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .timeout_ms = IFTTT_TIMEOUT_MS,
        .is_async = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");

    const char *post_data = "{}";
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Webhook sent successfully (HTTP %d)", status_code);
    } else {
        ESP_LOGE(TAG, "Webhook failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void webhook_task(void *pvParameters)
{
    uint8_t trigger_signal;

    ESP_LOGI(TAG, "Webhook task started (priority %d)", TASK_PRIORITY_IFTTT);

    while (1) {
        if (xQueueReceive(webhookQueue, &trigger_signal, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Emergency webhook triggered!");
            send_webhook_post();
        }
    }
}

void ifttt_webhook_init(void)
{
#if IFTTT_ENABLED
    ESP_LOGI(TAG, "Initializing IFTTT webhook notification...");
    ESP_LOGI(TAG, "URL: %s", IFTTT_WEBHOOK_URL);

    webhookQueue = xQueueCreate(QUEUE_SIZE_IFTTT, sizeof(uint8_t));
    if (webhookQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create webhook queue");
        return;
    }

    BaseType_t result = xTaskCreate(
        webhook_task,
        "webhook_task",
        TASK_STACK_IFTTT,
        NULL,
        TASK_PRIORITY_IFTTT,
        &webhookTaskHandle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create webhook task");
        vQueueDelete(webhookQueue);
        webhookQueue = NULL;
        return;
    }

    ESP_LOGI(TAG, "IFTTT webhook initialized successfully");
#else
    ESP_LOGI(TAG, "IFTTT webhook disabled in config");
#endif
}

void ifttt_webhook_trigger(void)
{
#if IFTTT_ENABLED
    if (webhookQueue != NULL) {
        uint8_t signal = 1;
        if (xQueueSend(webhookQueue, &signal, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Webhook queue full, skipping trigger");
        }
    }
#endif
}
