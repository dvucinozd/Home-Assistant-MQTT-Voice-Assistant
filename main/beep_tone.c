/**
 * Beep Tone Generator Implementation
 */

#include "beep_tone.h"
#include "bsp_board_extra.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "beep_tone";

#define BEEP_SAMPLE_RATE 16000 // 16kHz sample rate
#define PI 3.14159265358979323846

/**
 * Generate and play a simple sine wave beep tone
 */
esp_err_t beep_tone_play(uint16_t frequency, uint16_t duration,
                         uint8_t volume) {
  if (frequency < 100 || frequency > 4000) {
    ESP_LOGE(TAG, "Invalid frequency: %d Hz (range: 100-4000)", frequency);
    return ESP_ERR_INVALID_ARG;
  }

  if (duration < 50 || duration > 1000) {
    ESP_LOGE(TAG, "Invalid duration: %d ms (range: 50-1000)", duration);
    return ESP_ERR_INVALID_ARG;
  }

  if (volume > 100) {
    ESP_LOGE(TAG, "Invalid volume: %d (range: 0-100)", volume);
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "Playing beep: %d Hz, %d ms, vol=%d%%", frequency, duration,
           volume);

  // Calculate number of samples
  uint32_t num_samples = (BEEP_SAMPLE_RATE * duration) / 1000;

  // Allocate buffer for mono PCM samples (16-bit)
  int16_t *pcm_buffer = (int16_t *)malloc(num_samples * sizeof(int16_t));
  if (pcm_buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate PCM buffer");
    return ESP_ERR_NO_MEM;
  }

  // Generate sine wave with envelope (fade in/out to avoid clicks)
  float amplitude =
      (volume / 100.0f) * 16000.0f; // Max amplitude for 16-bit audio
  float fade_samples = BEEP_SAMPLE_RATE * 0.005f; // 5ms fade in/out

  for (uint32_t i = 0; i < num_samples; i++) {
    // Generate sine wave
    float t = (float)i / BEEP_SAMPLE_RATE;
    float sample = sinf(2.0f * PI * frequency * t);

    // Apply envelope (fade in/out)
    float envelope = 1.0f;
    if (i < fade_samples) {
      // Fade in
      envelope = (float)i / fade_samples;
    } else if (i > num_samples - fade_samples) {
      // Fade out
      envelope = (float)(num_samples - i) / fade_samples;
    }

    // Apply amplitude and envelope
    pcm_buffer[i] = (int16_t)(sample * amplitude * envelope);
  }

  // Configure codec for playback (16kHz MONO for beep)
  esp_err_t ret =
      bsp_extra_codec_set_fs(BEEP_SAMPLE_RATE, 16, I2S_SLOT_MODE_MONO);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure codec: %s", esp_err_to_name(ret));
    free(pcm_buffer);
    return ret;
  }

  // Unmute codec
  bsp_extra_codec_mute_set(false);

  // Write PCM data to I2S
  size_t bytes_to_write = num_samples * sizeof(int16_t);
  size_t bytes_written = 0;
  ret = bsp_extra_i2s_write(pcm_buffer, bytes_to_write, &bytes_written, 1000);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
    free(pcm_buffer);
    return ret;
  }

  ESP_LOGD(TAG, "Beep playback complete: %d samples, %d bytes written",
           num_samples, bytes_written);

  free(pcm_buffer);
  return ESP_OK;
}
