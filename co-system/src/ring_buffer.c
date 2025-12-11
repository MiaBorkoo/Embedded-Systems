#include "ring_buffer.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "RING_BUF";

// Static array - no heap allocation (embedded systems best practice)
static RingBufferItem_t buffer[RING_BUFFER_SIZE];

// Head and tail pointers (lecture W4L5)
// head = where next item will be written
// tail = where oldest item is (next to be read)
static int head = 0;
static int tail = 0;
static int count = 0;

// Mutex for thread safety (lecture W5L9 - use mutex for priority inheritance)
static SemaphoreHandle_t buffer_mutex = NULL;

// Mutex timeout - don't block forever
#define MUTEX_TIMEOUT_MS 100

void ring_buffer_init(void)
{
    // Create mutex (not binary semaphore - mutex has priority inheritance)
    buffer_mutex = xSemaphoreCreateMutex();

    // Reset pointers
    head = 0;
    tail = 0;
    count = 0;

    ESP_LOGI(TAG, "Ring buffer initialized (size: %d items)", RING_BUFFER_SIZE);
}

bool ring_buffer_push(RingBufferItem_t *item)
{
    if (item == NULL || buffer_mutex == NULL) {
        return false;
    }

    // Take mutex with timeout
    if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for push");
        return false;
    }

    // Check if buffer is full - overwrite oldest (lecture W4L5: "overwrite oldest may be better for real time data")
    if (count == RING_BUFFER_SIZE) {
        ESP_LOGW(TAG, "Buffer full, overwriting oldest data");
        // Move tail forward (discard oldest)
        tail = (tail + 1) % RING_BUFFER_SIZE;
        count--;
    }

    // Copy item to buffer at head position
    memcpy(&buffer[head], item, sizeof(RingBufferItem_t));

    // Advance head pointer (lecture W4L5: "head = (head + 1) % SIZE")
    head = (head + 1) % RING_BUFFER_SIZE;
    count++;

    // Release mutex (same task that locks should unlock - W5L9)
    xSemaphoreGive(buffer_mutex);

    return true;
}

bool ring_buffer_pop(RingBufferItem_t *item)
{
    if (item == NULL || buffer_mutex == NULL) {
        return false;
    }

    // Take mutex with timeout
    if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for pop");
        return false;
    }

    // Check if buffer is empty
    if (count == 0) {
        xSemaphoreGive(buffer_mutex);
        return false;
    }

    // Copy oldest item (at tail) to output
    memcpy(item, &buffer[tail], sizeof(RingBufferItem_t));

    // Advance tail pointer
    tail = (tail + 1) % RING_BUFFER_SIZE;
    count--;

    // Release mutex
    xSemaphoreGive(buffer_mutex);

    return true;
}

bool ring_buffer_is_empty(void)
{
    if (buffer_mutex == NULL) {
        return true;
    }

    bool empty = true;

    if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        empty = (count == 0);
        xSemaphoreGive(buffer_mutex);
    }

    return empty;
}

int ring_buffer_count(void)
{
    if (buffer_mutex == NULL) {
        return 0;
    }

    int current_count = 0;

    if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        current_count = count;
        xSemaphoreGive(buffer_mutex);
    }

    return current_count;
}

void ring_buffer_clear(void)
{
    if (buffer_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        head = 0;
        tail = 0;
        count = 0;
        xSemaphoreGive(buffer_mutex);
        ESP_LOGI(TAG, "Ring buffer cleared");
    }
}
