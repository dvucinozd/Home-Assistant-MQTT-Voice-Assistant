/**
 * @file agc.c
 * @brief Automatic Gain Control Implementation
 *
 * RMS-based AGC with smooth gain ramping for consistent audio levels.
 */

#include "agc.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>


static const char *TAG = "agc";

/**
 * @brief AGC instance structure
 */
struct agc_instance {
  agc_config_t config;

  // Current state
  float current_gain;    ///< Current gain multiplier
  uint16_t input_level;  ///< Last measured input RMS
  uint16_t output_level; ///< Last measured output RMS

  // Smoothing coefficients (calculated from attack/release times)
  float attack_coeff;  ///< Coefficient for gain increase
  float release_coeff; ///< Coefficient for gain decrease

  // Statistics
  uint32_t frames_processed;
  uint32_t clipping_count;
};

/**
 * @brief Calculate smoothing coefficient from time constant
 */
static float calculate_coefficient(float time_ms, uint32_t sample_rate,
                                   size_t frame_size) {
  if (time_ms <= 0) {
    return 1.0f; // Instant response
  }

  // Time constant in samples
  float time_samples = (time_ms / 1000.0f) * sample_rate;

  // Frames per time constant
  float frames = time_samples / frame_size;

  if (frames <= 0) {
    return 1.0f;
  }

  // Exponential smoothing coefficient
  return 1.0f - expf(-1.0f / frames);
}

/**
 * @brief Calculate RMS energy of audio frame
 */
static uint16_t calculate_rms(const int16_t *audio_data, size_t num_samples) {
  if (audio_data == NULL || num_samples == 0) {
    return 0;
  }

  uint64_t sum_squares = 0;

  for (size_t i = 0; i < num_samples; i++) {
    int32_t sample = audio_data[i];
    sum_squares += (uint64_t)(sample * sample);
  }

  uint32_t mean_square = (uint32_t)(sum_squares / num_samples);
  uint16_t rms = (uint16_t)sqrtf((float)mean_square);

  return rms;
}

agc_config_t agc_get_default_config(void) {
  agc_config_t config = {
      .sample_rate = 16000,
      .target_level = 4000,      // ~12% of full scale
      .min_gain = 0.5f,          // -6dB minimum
      .max_gain = 8.0f,          // +18dB maximum
      .attack_time_ms = 50.0f,   // Fast attack
      .release_time_ms = 200.0f, // Slower release
      .noise_gate_threshold = 50 // Low noise gate
  };
  return config;
}

esp_err_t agc_init(const agc_config_t *config, agc_handle_t *handle) {
  if (config == NULL || handle == NULL) {
    ESP_LOGE(TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  struct agc_instance *agc =
      (struct agc_instance *)calloc(1, sizeof(struct agc_instance));
  if (agc == NULL) {
    ESP_LOGE(TAG, "Failed to allocate AGC instance");
    return ESP_ERR_NO_MEM;
  }

  // Copy configuration
  memcpy(&agc->config, config, sizeof(agc_config_t));

  // Initialize state
  agc->current_gain = 1.0f;
  agc->input_level = 0;
  agc->output_level = 0;
  agc->frames_processed = 0;
  agc->clipping_count = 0;

  // Calculate smoothing coefficients
  // Assuming typical frame size of 512 samples
  const size_t typical_frame_size = 512;
  agc->attack_coeff = calculate_coefficient(
      config->attack_time_ms, config->sample_rate, typical_frame_size);
  agc->release_coeff = calculate_coefficient(
      config->release_time_ms, config->sample_rate, typical_frame_size);

  *handle = agc;

  ESP_LOGI(TAG, "AGC initialized:");
  ESP_LOGI(TAG, "  Target level: %u", config->target_level);
  ESP_LOGI(TAG, "  Gain range: %.2f - %.2f", config->min_gain,
           config->max_gain);
  ESP_LOGI(TAG, "  Attack: %.1fms, Release: %.1fms", config->attack_time_ms,
           config->release_time_ms);
  ESP_LOGI(TAG, "  Noise gate: %u", config->noise_gate_threshold);

  return ESP_OK;
}

esp_err_t agc_process(agc_handle_t handle, int16_t *audio_data,
                      size_t num_samples) {
  if (handle == NULL || audio_data == NULL || num_samples == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  struct agc_instance *agc = (struct agc_instance *)handle;

  // Calculate input RMS
  uint16_t input_rms = calculate_rms(audio_data, num_samples);
  agc->input_level = input_rms;

  // Apply noise gate
  if (input_rms < agc->config.noise_gate_threshold) {
    // Below noise gate - zero out audio (optional: could just skip gain
    // adjustment) For now, we'll just not adjust gain below noise floor
    agc->frames_processed++;
    return ESP_OK;
  }

  // Calculate desired gain
  float desired_gain = 1.0f;
  if (input_rms > 0) {
    desired_gain = (float)agc->config.target_level / (float)input_rms;
  }

  // Clamp desired gain to limits
  if (desired_gain < agc->config.min_gain) {
    desired_gain = agc->config.min_gain;
  }
  if (desired_gain > agc->config.max_gain) {
    desired_gain = agc->config.max_gain;
  }

  // Smooth gain transition
  float coeff;
  if (desired_gain > agc->current_gain) {
    // Gain increasing (attack)
    coeff = agc->attack_coeff;
  } else {
    // Gain decreasing (release)
    coeff = agc->release_coeff;
  }

  agc->current_gain =
      agc->current_gain + coeff * (desired_gain - agc->current_gain);

  // Apply gain to audio
  bool clipped = false;
  for (size_t i = 0; i < num_samples; i++) {
    int32_t sample = (int32_t)audio_data[i];
    int32_t amplified = (int32_t)(sample * agc->current_gain);

    // Soft clipping
    if (amplified > 32767) {
      amplified = 32767;
      clipped = true;
    } else if (amplified < -32768) {
      amplified = -32768;
      clipped = true;
    }

    audio_data[i] = (int16_t)amplified;
  }

  if (clipped) {
    agc->clipping_count++;
  }

  // Calculate output RMS
  agc->output_level = calculate_rms(audio_data, num_samples);

  agc->frames_processed++;

  // Log every 100 frames (~3 seconds at 16kHz with 512 sample frames)
  if (agc->frames_processed % 100 == 0) {
    ESP_LOGD(TAG, "AGC: in=%u, out=%u, gain=%.2f, clips=%lu", agc->input_level,
             agc->output_level, agc->current_gain, agc->clipping_count);
  }

  return ESP_OK;
}

float agc_get_current_gain(agc_handle_t handle) {
  if (handle == NULL) {
    return 1.0f;
  }

  struct agc_instance *agc = (struct agc_instance *)handle;
  return agc->current_gain;
}

uint16_t agc_get_input_level(agc_handle_t handle) {
  if (handle == NULL) {
    return 0;
  }

  struct agc_instance *agc = (struct agc_instance *)handle;
  return agc->input_level;
}

esp_err_t agc_set_target_level(agc_handle_t handle, uint16_t target_level) {
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  struct agc_instance *agc = (struct agc_instance *)handle;
  agc->config.target_level = target_level;

  ESP_LOGI(TAG, "AGC target level updated to %u", target_level);
  return ESP_OK;
}

esp_err_t agc_set_gain_limits(agc_handle_t handle, float min_gain,
                              float max_gain) {
  if (handle == NULL || min_gain <= 0 || max_gain < min_gain) {
    return ESP_ERR_INVALID_ARG;
  }

  struct agc_instance *agc = (struct agc_instance *)handle;
  agc->config.min_gain = min_gain;
  agc->config.max_gain = max_gain;

  // Clamp current gain if needed
  if (agc->current_gain < min_gain) {
    agc->current_gain = min_gain;
  }
  if (agc->current_gain > max_gain) {
    agc->current_gain = max_gain;
  }

  ESP_LOGI(TAG, "AGC gain limits updated: %.2f - %.2f", min_gain, max_gain);
  return ESP_OK;
}

void agc_reset(agc_handle_t handle) {
  if (handle == NULL) {
    return;
  }

  struct agc_instance *agc = (struct agc_instance *)handle;
  agc->current_gain = 1.0f;
  agc->input_level = 0;
  agc->output_level = 0;
  agc->clipping_count = 0;

  ESP_LOGD(TAG, "AGC reset");
}

void agc_deinit(agc_handle_t handle) {
  if (handle != NULL) {
    free(handle);
    ESP_LOGI(TAG, "AGC deinitialized");
  }
}
