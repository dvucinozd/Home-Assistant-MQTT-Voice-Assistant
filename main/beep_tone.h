/**
 * Beep Tone Generator
 * Generates simple audio feedback tones
 */

#ifndef BEEP_TONE_H
#define BEEP_TONE_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Play a short beep tone as audio feedback
 *
 * Generates and plays a simple sine wave tone to indicate wake word detection.
 * This provides audio feedback to the user that the system is listening.
 *
 * @param frequency Frequency of the beep in Hz (e.g., 800, 1000, 1200)
 * @param duration Duration of the beep in milliseconds (e.g., 100, 150, 200)
 * @param volume Volume level 0-100 (e.g., 30 for quiet beep)
 * @return ESP_OK on success
 */
esp_err_t beep_tone_play(uint16_t frequency, uint16_t duration, uint8_t volume);

#ifdef __cplusplus
}
#endif

#endif /* BEEP_TONE_H */
