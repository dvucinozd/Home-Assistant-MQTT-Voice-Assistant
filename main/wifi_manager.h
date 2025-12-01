/**
 * WiFi Manager Header
 * ESP32-P4 Voice Assistant
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi in station mode and connect to configured AP
 *
 * This function initializes the WiFi subsystem using ESP32-C6 coprocessor
 * via SDIO interface (esp_wifi_remote). It will block until connection
 * is established or max retries reached.
 *
 * @return
 *    - ESP_OK: WiFi connected successfully
 *    - ESP_FAIL: Failed to connect after max retries
 *    - ESP_ERR_TIMEOUT: Unexpected event
 */
esp_err_t wifi_init_sta(void);

/**
 * @brief Check if WiFi is currently connected
 *
 * @return true if connected to AP and has IP address, false otherwise
 */
bool wifi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
