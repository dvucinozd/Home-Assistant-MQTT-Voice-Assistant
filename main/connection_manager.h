/**
 * @file connection_manager.h
 * @brief Centralized connection management with auto-reconnection
 *
 * Manages Home Assistant WebSocket and MQTT connections with:
 * - Health monitoring
 * - Automatic reconnection with exponential backoff
 * - Connection state callbacks
 */

#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connection types
 */
typedef enum {
  CONN_TYPE_HA_WEBSOCKET, ///< Home Assistant WebSocket
  CONN_TYPE_MQTT,         ///< MQTT broker
  CONN_TYPE_MAX
} connection_type_t;

/**
 * @brief Connection states
 */
typedef enum {
  CONN_STATE_DISCONNECTED, ///< Not connected
  CONN_STATE_CONNECTING,   ///< Connection in progress
  CONN_STATE_CONNECTED,    ///< Connected and authenticated
  CONN_STATE_ERROR,        ///< Connection error
  CONN_STATE_DISABLED      ///< Manually disabled
} connection_state_t;

/**
 * @brief Connection status callback
 *
 * @param type Connection type
 * @param state New connection state
 * @param retry_count Number of reconnection attempts
 */
typedef void (*connection_status_callback_t)(connection_type_t type,
                                             connection_state_t state,
                                             int retry_count);

/**
 * @brief Reconnection handler function type
 *
 * Called by connection manager to perform actual reconnection.
 *
 * @return ESP_OK if reconnection successful
 */
typedef esp_err_t (*connection_reconnect_fn_t)(void);

/**
 * @brief Connection manager configuration
 */
typedef struct {
  uint32_t health_check_interval_ms; ///< Interval between health checks
                                     ///< (default: 30s)
  uint32_t initial_retry_delay_ms;   ///< Initial delay before first retry
                                     ///< (default: 1s)
  uint32_t max_retry_delay_ms; ///< Maximum delay between retries (default: 60s)
  uint8_t max_retry_count;     ///< Max retries before giving up (0 = infinite)
  float backoff_multiplier; ///< Exponential backoff multiplier (default: 2.0)
} connection_manager_config_t;

/**
 * @brief Get default configuration
 *
 * @return Default connection manager configuration
 */
connection_manager_config_t connection_manager_get_default_config(void);

/**
 * @brief Initialize connection manager
 *
 * @param config Connection manager configuration
 * @return ESP_OK on success
 */
esp_err_t connection_manager_init(const connection_manager_config_t *config);

/**
 * @brief Register a connection for management
 *
 * @param type Connection type
 * @param name Human-readable name for logging
 * @param reconnect_fn Function to call for reconnection
 * @return ESP_OK on success
 */
esp_err_t connection_manager_register(connection_type_t type, const char *name,
                                      connection_reconnect_fn_t reconnect_fn);

/**
 * @brief Update connection state
 *
 * Called by connection modules when state changes.
 *
 * @param type Connection type
 * @param state New connection state
 */
void connection_manager_update_state(connection_type_t type,
                                     connection_state_t state);

/**
 * @brief Get current connection state
 *
 * @param type Connection type
 * @return Current connection state
 */
connection_state_t connection_manager_get_state(connection_type_t type);

/**
 * @brief Check if all registered connections are connected
 *
 * @return true if all connections are in CONNECTED state
 */
bool connection_manager_all_connected(void);

/**
 * @brief Request immediate reconnection
 *
 * @param type Connection type
 * @return ESP_OK if reconnection initiated
 */
esp_err_t connection_manager_request_reconnect(connection_type_t type);

/**
 * @brief Register status callback
 *
 * @param callback Function to call on connection state changes
 */
void connection_manager_register_callback(
    connection_status_callback_t callback);

/**
 * @brief Get retry count for a connection
 *
 * @param type Connection type
 * @return Number of reconnection attempts since last successful connection
 */
int connection_manager_get_retry_count(connection_type_t type);

/**
 * @brief Start connection manager monitoring task
 *
 * @return ESP_OK on success
 */
esp_err_t connection_manager_start(void);

/**
 * @brief Stop connection manager
 */
void connection_manager_stop(void);

/**
 * @brief Deinitialize connection manager
 */
void connection_manager_deinit(void);

/**
 * @brief Convert connection type to string
 *
 * @param type Connection type
 * @return String representation
 */
const char *connection_manager_type_to_string(connection_type_t type);

/**
 * @brief Convert connection state to string
 *
 * @param state Connection state
 * @return String representation
 */
const char *connection_manager_state_to_string(connection_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* CONNECTION_MANAGER_H */
