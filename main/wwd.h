#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the wake word detection threshold at runtime.
 *
 * @param threshold Threshold value (e.g., 0.5 to 0.95).
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t wwd_set_threshold(float threshold);

/**
 * @brief Get the current wake word detection threshold.
 *
 * @return Current threshold value.
 */
float wwd_get_threshold(void);

#ifdef __cplusplus
}
#endif
