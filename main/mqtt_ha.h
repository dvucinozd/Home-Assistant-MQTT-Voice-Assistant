/**
 * MQTT Home Assistant Integration
 *
 * Provides MQTT Discovery protocol for Home Assistant integration.
 * Exposes device sensors, switches, and controls as HA entities.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MQTT Configuration
 */
typedef struct {
    const char *broker_uri;        // MQTT broker URI (e.g., "mqtt://homeassistant.local:1883")
    const char *username;          // MQTT username (optional)
    const char *password;          // MQTT password (optional)
    const char *client_id;         // MQTT client ID
} mqtt_ha_config_t;

/**
 * Entity types supported by Home Assistant MQTT Discovery
 */
typedef enum {
    MQTT_HA_SENSOR,      // Read-only sensor (e.g., WiFi RSSI, uptime)
    MQTT_HA_SWITCH,      // Binary switch (e.g., enable/disable WWD)
    MQTT_HA_NUMBER,      // Number input (e.g., VAD threshold, mic gain)
    MQTT_HA_SELECT,      // Dropdown selection (e.g., WWD threshold presets)
    MQTT_HA_BUTTON       // Action button (e.g., restart, test TTS)
} mqtt_ha_entity_type_t;

/**
 * Command callback for entities (switch, number, select, button)
 */
typedef void (*mqtt_ha_command_callback_t)(const char *entity_id, const char *payload);

/**
 * Initialize MQTT Home Assistant client
 *
 * @param config MQTT configuration
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_init(const mqtt_ha_config_t *config);

/**
 * Start MQTT client and publish discovery messages
 *
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_start(void);

/**
 * Stop MQTT client
 *
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_stop(void);

/**
 * Register a sensor entity with Home Assistant
 *
 * @param entity_id Unique entity ID (e.g., "wifi_rssi")
 * @param name Friendly name (e.g., "WiFi Signal")
 * @param unit Unit of measurement (e.g., "dBm", "%", "MB")
 * @param device_class HA device class (e.g., "signal_strength", "duration")
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_register_sensor(const char *entity_id, const char *name,
                                   const char *unit, const char *device_class);

/**
 * Register a switch entity with Home Assistant
 *
 * @param entity_id Unique entity ID (e.g., "wwd_enabled")
 * @param name Friendly name (e.g., "Wake Word Detection")
 * @param callback Command callback when switch is toggled
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_register_switch(const char *entity_id, const char *name,
                                   mqtt_ha_command_callback_t callback);

/**
 * Register a number entity with Home Assistant
 *
 * @param entity_id Unique entity ID (e.g., "vad_threshold")
 * @param name Friendly name (e.g., "VAD Threshold")
 * @param min Minimum value
 * @param max Maximum value
 * @param step Step increment
 * @param unit Unit (optional, e.g., "dB")
 * @param callback Command callback when value changes
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_register_number(const char *entity_id, const char *name,
                                   float min, float max, float step, const char *unit,
                                   mqtt_ha_command_callback_t callback);

/**
 * Register a select entity with Home Assistant
 *
 * @param entity_id Unique entity ID (e.g., "wwd_threshold")
 * @param name Friendly name (e.g., "WWD Threshold")
 * @param options Comma-separated options (e.g., "0.5,0.6,0.7,0.8,0.9,0.95")
 * @param callback Command callback when selection changes
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_register_select(const char *entity_id, const char *name,
                                   const char *options, mqtt_ha_command_callback_t callback);

/**
 * Register a button entity with Home Assistant
 *
 * @param entity_id Unique entity ID (e.g., "restart")
 * @param name Friendly name (e.g., "Restart Device")
 * @param callback Command callback when button is pressed
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_register_button(const char *entity_id, const char *name,
                                   mqtt_ha_command_callback_t callback);

/**
 * Update sensor state value
 *
 * @param entity_id Entity ID
 * @param value Value as string (e.g., "-45", "28.5")
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_update_sensor(const char *entity_id, const char *value);

/**
 * Update switch state
 *
 * @param entity_id Entity ID
 * @param state true = ON, false = OFF
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_update_switch(const char *entity_id, bool state);

/**
 * Update number state
 *
 * @param entity_id Entity ID
 * @param value Current value
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_update_number(const char *entity_id, float value);

/**
 * Update select state
 *
 * @param entity_id Entity ID
 * @param option Current selected option
 * @return ESP_OK on success
 */
esp_err_t mqtt_ha_update_select(const char *entity_id, const char *option);

/**
 * Check if MQTT is connected
 *
 * @return true if connected
 */
bool mqtt_ha_is_connected(void);

#ifdef __cplusplus
}
#endif
