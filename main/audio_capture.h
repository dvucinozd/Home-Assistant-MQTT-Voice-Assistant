/**
 * Audio Capture Module
 * Handles microphone input via ES8311 codec
 */

#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for captured audio data
 *
 * @param audio_data PCM audio buffer (16-bit, 16kHz, mono)
 * @param length Length of audio data in bytes
 */
typedef void (*audio_capture_callback_t)(const uint8_t *audio_data, size_t length);

/**
 * @brief Initialize audio capture
 *
 * @return ESP_OK on success
 */
esp_err_t audio_capture_init(void);

/**
 * @brief Start capturing audio
 *
 * @param callback Function to call with captured audio chunks
 * @return ESP_OK on success
 */
esp_err_t audio_capture_start(audio_capture_callback_t callback);

/**
 * @brief Stop capturing audio
 */
void audio_capture_stop(void);

/**
 * @brief Deinitialize audio capture
 */
void audio_capture_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_CAPTURE_H */
