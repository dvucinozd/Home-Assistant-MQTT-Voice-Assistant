/**
 * Wake Prompt Player
 * Plays a pre-recorded audio prompt when wake word is detected
 */

#ifndef WAKE_PROMPT_H
#define WAKE_PROMPT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize wake prompt player
 * Loads the wake prompt audio file from SD card
 *
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if audio file missing
 */
esp_err_t wake_prompt_init(void);

/**
 * @brief Play the wake prompt audio
 * This is a blocking call - returns after audio playback completes
 *
 * @return ESP_OK on success
 */
esp_err_t wake_prompt_play(void);

/**
 * @brief Check if wake prompt is available
 *
 * @return true if audio file is loaded and ready
 */
bool wake_prompt_is_available(void);

#ifdef __cplusplus
}
#endif

#endif /* WAKE_PROMPT_H */
