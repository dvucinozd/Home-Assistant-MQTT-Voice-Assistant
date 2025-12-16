/**
 * Wake Word Detection (WWD) Module
 *
 * Uses ESP-SR WakeNet9l for hands-free voice assistant activation.
 * Continuously listens for wake word and triggers VAD+STT pipeline.
 */

#ifndef WWD_H
#define WWD_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wake word detection events
 */
typedef enum {
  WWD_EVENT_DETECTED, // Wake word detected
  WWD_EVENT_TIMEOUT,  // No wake word within timeout
  WWD_EVENT_ERROR     // Error occurred
} wwd_event_t;

/**
 * Wake word detection callback
 *
 * Called when wake word is detected or error occurs.
 *
 * @param event Event type
 * @param user_data User data passed during init
 */
typedef void (*wwd_callback_t)(wwd_event_t event, void *user_data);

/**
 * Wake word detection configuration
 */
typedef struct {
  uint32_t sample_rate;      // Audio sample rate (default: 16000 Hz)
  uint32_t bit_width;        // Bits per sample (default: 16)
  uint32_t channels;         // Number of channels (default: 1 - mono)
  float detection_threshold; // Detection threshold 0.0-1.0 (default: 0.5)
  wwd_callback_t callback;   // Callback function
  void *user_data;           // User data for callback
} wwd_config_t;

/**
 * Get default WWD configuration
 *
 * @return Default configuration struct
 */
wwd_config_t wwd_get_default_config(void);

/**
 * Initialize wake word detection
 *
 * Initializes WakeNet model and prepares for detection.
 *
 * @param config Configuration parameters
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wwd_init(const wwd_config_t *config);

/**
 * Start wake word detection
 *
 * Begins continuous listening for wake word.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wwd_start(void);

/**
 * Stop wake word detection
 *
 * Stops listening and frees resources.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wwd_stop(void);

/**
 * Feed audio data to wake word detector
 *
 * Process audio chunk for wake word detection.
 * This should be called from audio capture task.
 *
 * @param audio_data PCM audio samples
 * @param length Number of samples (int16_t)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wwd_feed_audio(const int16_t *audio_data, size_t length);

/**
 * Set detection threshold at runtime (without reinit)
 *
 * Updates threshold for all wake words. Valid range: 0.4 - 0.95
 * Values outside range are clamped automatically.
 *
 * @param threshold Detection threshold (0.4-0.95)
 * @return ESP_OK on success, ESP_FAIL if set failed, ESP_ERR_INVALID_STATE if
 * not initialized
 */
esp_err_t wwd_set_threshold(float threshold);

/**
 * Get current detection threshold
 *
 * @return Current threshold value (0.0 if not initialized)
 */
float wwd_get_threshold(void);

/**
 * Check if wake word detection is running
 *
 * @return true if running, false otherwise
 */
bool wwd_is_running(void);

/**
 * Deinitialize wake word detection
 *
 * Cleanup and free all resources.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wwd_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // WWD_H
