/**
 * @file ota_update.h
 * @brief OTA (Over-The-Air) firmware update module
 *
 * Provides HTTP-based OTA updates with progress tracking and rollback support.
 */

#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA update state
 */
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_SUCCESS,
    OTA_STATE_FAILED
} ota_state_t;

/**
 * @brief OTA progress callback
 *
 * @param state Current OTA state
 * @param progress Progress percentage (0-100)
 * @param message Status message
 */
typedef void (*ota_progress_callback_t)(ota_state_t state, int progress, const char *message);

/**
 * @brief Initialize OTA update module
 *
 * @return ESP_OK on success
 */
esp_err_t ota_update_init(void);

/**
 * @brief Start OTA update from HTTP URL
 *
 * Downloads firmware from specified URL and updates the device.
 * Device will reboot automatically on successful update.
 *
 * @param url HTTP/HTTPS URL to firmware binary (.bin file)
 * @return ESP_OK if update started successfully
 */
esp_err_t ota_update_start(const char *url);

/**
 * @brief Check if OTA update is in progress
 *
 * @return true if update is running
 */
bool ota_update_is_running(void);

/**
 * @brief Get current OTA state
 *
 * @return Current OTA state
 */
ota_state_t ota_update_get_state(void);

/**
 * @brief Get current OTA progress
 *
 * @return Progress percentage (0-100)
 */
int ota_update_get_progress(void);

/**
 * @brief Register progress callback
 *
 * @param callback Callback function
 */
void ota_update_register_callback(ota_progress_callback_t callback);

/**
 * @brief Get current firmware version
 *
 * @return Version string (from app_desc)
 */
const char* ota_update_get_current_version(void);

/**
 * @brief Check if partition was rolled back
 *
 * Checks if device booted from fallback partition due to failed update.
 *
 * @return true if rolled back
 */
bool ota_update_check_rollback(void);

/**
 * @brief Mark current partition as valid
 *
 * Must be called after successful boot to prevent rollback.
 * Usually called after all systems initialize successfully.
 *
 * @return ESP_OK on success
 */
esp_err_t ota_update_mark_valid(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_UPDATE_H
