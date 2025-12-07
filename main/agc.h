/**
 * @file agc.h
 * @brief Automatic Gain Control (AGC) for microphone input
 *
 * Provides dynamic gain adjustment based on RMS energy levels
 * to normalize audio amplitude for consistent wake word detection
 * and speech recognition.
 */

#ifndef AGC_H
#define AGC_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AGC configuration parameters
 */
typedef struct {
    uint32_t sample_rate;           ///< Audio sample rate (Hz)
    uint16_t target_level;          ///< Target RMS amplitude (0-32767)
    float min_gain;                 ///< Minimum gain multiplier (e.g., 0.1)
    float max_gain;                 ///< Maximum gain multiplier (e.g., 10.0)
    float attack_time_ms;           ///< Attack time for gain increase (ms)
    float release_time_ms;          ///< Release time for gain decrease (ms)
    uint16_t noise_gate_threshold;  ///< Below this level, apply noise gate (0 to disable)
} agc_config_t;

/**
 * @brief AGC handle type
 */
typedef struct agc_instance* agc_handle_t;

/**
 * @brief Get default AGC configuration
 *
 * @return Default configuration suitable for voice input
 */
agc_config_t agc_get_default_config(void);

/**
 * @brief Initialize AGC module
 *
 * @param config AGC configuration
 * @param handle Pointer to receive AGC handle
 * @return ESP_OK on success
 */
esp_err_t agc_init(const agc_config_t *config, agc_handle_t *handle);

/**
 * @brief Process audio frame through AGC
 *
 * Audio is modified in-place with applied gain.
 *
 * @param handle AGC handle
 * @param audio_data Audio samples (modified in-place)
 * @param num_samples Number of samples
 * @return ESP_OK on success
 */
esp_err_t agc_process(agc_handle_t handle, int16_t *audio_data, size_t num_samples);

/**
 * @brief Get current gain value
 *
 * @param handle AGC handle
 * @return Current gain multiplier
 */
float agc_get_current_gain(agc_handle_t handle);

/**
 * @brief Get current RMS level (before gain)
 *
 * @param handle AGC handle
 * @return Current input RMS level
 */
uint16_t agc_get_input_level(agc_handle_t handle);

/**
 * @brief Set target level dynamically
 *
 * @param handle AGC handle
 * @param target_level New target RMS level
 * @return ESP_OK on success
 */
esp_err_t agc_set_target_level(agc_handle_t handle, uint16_t target_level);

/**
 * @brief Set gain limits dynamically
 *
 * @param handle AGC handle
 * @param min_gain Minimum gain multiplier
 * @param max_gain Maximum gain multiplier
 * @return ESP_OK on success
 */
esp_err_t agc_set_gain_limits(agc_handle_t handle, float min_gain, float max_gain);

/**
 * @brief Reset AGC state
 *
 * Resets gain to 1.0 and clears history.
 *
 * @param handle AGC handle
 */
void agc_reset(agc_handle_t handle);

/**
 * @brief Deinitialize AGC module
 *
 * @param handle AGC handle
 */
void agc_deinit(agc_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* AGC_H */
