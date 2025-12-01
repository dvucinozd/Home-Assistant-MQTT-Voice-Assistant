/**
 * Audio Capture Implementation
 * Uses BSP board I2S functions to read from ES8311 microphone
 */

#include "audio_capture.h"
#include "bsp_board_extra.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_capture";

#define CAPTURE_BUFFER_SIZE 1024   // Samples per read (2048 bytes for 16-bit)

static TaskHandle_t capture_task_handle = NULL;
static audio_capture_callback_t capture_callback = NULL;
static bool is_capturing = false;

/**
 * Capture task - continuously reads audio from I2S using BSP functions
 */
static void capture_task(void *arg)
{
    int16_t *buffer = (int16_t *)malloc(CAPTURE_BUFFER_SIZE * sizeof(int16_t));
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate capture buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Capture task started");

    int chunk_count = 0;
    while (is_capturing) {
        size_t bytes_read = 0;

        // Read audio data from I2S using BSP function
        esp_err_t ret = bsp_extra_i2s_read(buffer,
                                           CAPTURE_BUFFER_SIZE * sizeof(int16_t),
                                           &bytes_read,
                                           portMAX_DELAY);

        if (ret == ESP_OK && bytes_read > 0) {
            // Debug: Check if audio has any non-zero samples
            if (chunk_count % 10 == 0) {  // Log every 10th chunk
                int non_zero = 0;
                for (int i = 0; i < CAPTURE_BUFFER_SIZE; i++) {
                    if (buffer[i] != 0) non_zero++;
                }
                ESP_LOGI(TAG, "Chunk %d: %d bytes, %d non-zero samples",
                         chunk_count, bytes_read, non_zero);
            }
            chunk_count++;

            // Call callback with captured audio
            if (capture_callback) {
                capture_callback((const uint8_t *)buffer, bytes_read);
            }
        } else if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2S read failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    free(buffer);
    ESP_LOGI(TAG, "Capture task stopped");
    vTaskDelete(NULL);
}

esp_err_t audio_capture_init(void)
{
    ESP_LOGI(TAG, "Initializing audio capture (using BSP I2S)...");

    // BSP codec already initialized by bsp_extra_codec_init()
    // No need to create new I2S channel - just use existing one

    ESP_LOGI(TAG, "Audio capture initialized (16kHz, mono, 16-bit)");
    return ESP_OK;
}

esp_err_t audio_capture_start(audio_capture_callback_t callback)
{
    if (is_capturing) {
        ESP_LOGW(TAG, "Already capturing");
        return ESP_OK;
    }

    capture_callback = callback;
    is_capturing = true;

    // Create capture task
    BaseType_t task_ret = xTaskCreate(capture_task, "audio_capture", 4096, NULL, 5, &capture_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create capture task");
        is_capturing = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Audio capture started");
    return ESP_OK;
}

void audio_capture_stop(void)
{
    if (!is_capturing) {
        return;
    }

    is_capturing = false;
    capture_callback = NULL;

    // Wait for task to finish
    if (capture_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Give task time to exit
        capture_task_handle = NULL;
    }

    ESP_LOGI(TAG, "Audio capture stopped");
}

void audio_capture_deinit(void)
{
    audio_capture_stop();
    ESP_LOGI(TAG, "Audio capture deinitialized");
}
