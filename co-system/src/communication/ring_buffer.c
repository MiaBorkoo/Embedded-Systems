/**
 * ring_buffer.c - Circular buffer for offline telemetry storage
 *
 * Based on lecture W4L5 (FIFO/Queue/Ring Buffer) and W5L9 (Mutex)
 *
 * Why a ring buffer?
 * - Fixed memory (no malloc/free - critical for embedded)
 * - O(1) push and pop operations
 * - Overwrites oldest data when full (best for real-time sensor data)
 *
 * Thread Safety:
 * - Uses mutex (not binary semaphore) for priority inheritance
 * - Prevents priority inversion when high-priority task waits for buffer
 */

#include "ring_buffer.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "RINGBUF";

// Static array - no heap allocation (embedded systems best practice)
// Why static? Predictable memory usage, no fragmentation, faster access
static Telemetry_t buffer[RING_BUFFER_SIZE];

// Head and tail pointers (lecture W4L5)
// head = index where next item will be WRITTEN
// tail = index where next item will be READ (oldest item)
static int head = 0;
static int tail = 0;
static int count = 0;

// Mutex for thread safety (lecture W5L9)
// Why mutex not semaphore? Mutex has priority inheritance - prevents priority inversion
static SemaphoreHandle_t buffer_mutex = NULL;

// Mutex timeout - don't block forever (100ms is reasonable)
#define MUTEX_TIMEOUT_MS 100

/**
 * ring_buffer_init - Initialize the ring buffer
 *
 * MUST be called before any other ring buffer function.
 * Creates mutex and resets head/tail pointers.
 *
 * Interview note: We use xSemaphoreCreateMutex() not xSemaphoreCreateBinary()
 * because mutex provides priority inheritance (lecture W5L9).
 */
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

/**
 * ring_buffer_push - Add item to buffer (FIFO)
 *
 * @param item: Pointer to Telemetry_t to add
 * @return: true if successful, false if mutex timeout or NULL pointer
 *
 * Behavior when full: OVERWRITES oldest data (moves tail forward)
 * This is correct for real-time sensor data where recent data matters more.
 *
 * Lecture W4L5: "For real-time data, overwrite oldest may be better than blocking"
 */
bool ring_buffer_push(Telemetry_t *item)
{
    if (item == NULL || buffer_mutex == NULL) {
        return false;
    }

    // Take mutex with timeout - don't block forever
    if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for push");
        return false;
    }

    // Check if buffer is full - overwrite oldest
    if (count == RING_BUFFER_SIZE) {
        ESP_LOGW(TAG, "Buffer full, overwriting oldest data");
        // Move tail forward (discard oldest item)
        tail = (tail + 1) % RING_BUFFER_SIZE;
        count--;  // Will be incremented back below
    }

    // Copy item to buffer at head position
    memcpy(&buffer[head], item, sizeof(Telemetry_t));

    // Advance head pointer with wrap-around (lecture W4L5)
    // head = (head + 1) % SIZE ensures circular behavior
    head = (head + 1) % RING_BUFFER_SIZE;
    count++;

    // Release mutex (same task that locks must unlock - W5L9)
    xSemaphoreGive(buffer_mutex);

    return true;
}

/**
 * ring_buffer_pop - Remove oldest item from buffer (FIFO)
 *
 * @param item: Pointer to store the popped item
 * @return: true if item retrieved, false if empty or mutex timeout
 *
 * Interview note: We read from TAIL (oldest) not HEAD (newest)
 * This is FIFO - First In First Out
 */
bool ring_buffer_pop(Telemetry_t *item)
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
        return false;  // Nothing to pop
    }

    // Copy oldest item (at tail) to output
    memcpy(item, &buffer[tail], sizeof(Telemetry_t));

    // Advance tail pointer with wrap-around
    tail = (tail + 1) % RING_BUFFER_SIZE;
    count--;

    // Release mutex
    xSemaphoreGive(buffer_mutex);

    return true;
}

/**
 * ring_buffer_is_empty - Check if buffer has no items
 *
 * Thread-safe check using mutex.
 */
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

/**
 * ring_buffer_count - Get number of items in buffer
 *
 * Thread-safe count retrieval.
 * Used by agent_task to log how many items are buffered.
 */
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

/**
 * ring_buffer_clear - Empty the buffer
 *
 * Resets head, tail, and count to 0.
 * Does NOT zero the actual data (unnecessary overhead).
 */
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
