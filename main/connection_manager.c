/**
 * @file connection_manager.c
 * @brief Connection Manager Implementation
 *
 * Centralized connection management with automatic reconnection.
 */

#include "connection_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>


static const char *TAG = "conn_mgr";

/**
 * @brief Connection entry structure
 */
typedef struct {
  bool registered;
  const char *name;
  connection_state_t state;
  connection_reconnect_fn_t reconnect_fn;
  int retry_count;
  uint32_t next_retry_delay_ms;
  TickType_t last_attempt_time;
  bool reconnect_pending;
} connection_entry_t;

/**
 * @brief Connection manager state
 */
static struct {
  bool initialized;
  bool running;
  connection_manager_config_t config;
  connection_entry_t connections[CONN_TYPE_MAX];
  connection_status_callback_t status_callback;
  TaskHandle_t task_handle;
  SemaphoreHandle_t mutex;
} cm_state = {.initialized = false,
              .running = false,
              .status_callback = NULL,
              .task_handle = NULL,
              .mutex = NULL};

// String representations
static const char *type_strings[] = {
    [CONN_TYPE_HA_WEBSOCKET] = "HA_WebSocket", [CONN_TYPE_MQTT] = "MQTT"};

static const char *state_strings[] = {[CONN_STATE_DISCONNECTED] =
                                          "DISCONNECTED",
                                      [CONN_STATE_CONNECTING] = "CONNECTING",
                                      [CONN_STATE_CONNECTED] = "CONNECTED",
                                      [CONN_STATE_ERROR] = "ERROR",
                                      [CONN_STATE_DISABLED] = "DISABLED"};

connection_manager_config_t connection_manager_get_default_config(void) {
  connection_manager_config_t config = {
      .health_check_interval_ms = 30000, // 30 seconds
      .initial_retry_delay_ms = 1000,    // 1 second
      .max_retry_delay_ms = 60000,       // 60 seconds
      .max_retry_count = 0,              // Infinite retries
      .backoff_multiplier = 2.0f};
  return config;
}

esp_err_t connection_manager_init(const connection_manager_config_t *config) {
  if (cm_state.initialized) {
    ESP_LOGW(TAG, "Connection manager already initialized");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing connection manager...");

  // Create mutex
  cm_state.mutex = xSemaphoreCreateMutex();
  if (cm_state.mutex == NULL) {
    ESP_LOGE(TAG, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  // Copy configuration
  if (config != NULL) {
    memcpy(&cm_state.config, config, sizeof(connection_manager_config_t));
  } else {
    cm_state.config = connection_manager_get_default_config();
  }

  // Initialize connection entries
  for (int i = 0; i < CONN_TYPE_MAX; i++) {
    cm_state.connections[i].registered = false;
    cm_state.connections[i].name = NULL;
    cm_state.connections[i].state = CONN_STATE_DISCONNECTED;
    cm_state.connections[i].reconnect_fn = NULL;
    cm_state.connections[i].retry_count = 0;
    cm_state.connections[i].next_retry_delay_ms =
        cm_state.config.initial_retry_delay_ms;
    cm_state.connections[i].last_attempt_time = 0;
    cm_state.connections[i].reconnect_pending = false;
  }

  cm_state.initialized = true;

  ESP_LOGI(TAG, "Connection manager initialized");
  ESP_LOGI(TAG, "  Health check interval: %lu ms",
           cm_state.config.health_check_interval_ms);
  ESP_LOGI(TAG, "  Retry delay: %lu - %lu ms (x%.1f backoff)",
           cm_state.config.initial_retry_delay_ms,
           cm_state.config.max_retry_delay_ms,
           cm_state.config.backoff_multiplier);

  return ESP_OK;
}

esp_err_t connection_manager_register(connection_type_t type, const char *name,
                                      connection_reconnect_fn_t reconnect_fn) {
  if (!cm_state.initialized) {
    ESP_LOGE(TAG, "Connection manager not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (type >= CONN_TYPE_MAX) {
    ESP_LOGE(TAG, "Invalid connection type");
    return ESP_ERR_INVALID_ARG;
  }

  xSemaphoreTake(cm_state.mutex, portMAX_DELAY);

  cm_state.connections[type].registered = true;
  cm_state.connections[type].name = name ? name : type_strings[type];
  cm_state.connections[type].reconnect_fn = reconnect_fn;
  cm_state.connections[type].state = CONN_STATE_DISCONNECTED;
  cm_state.connections[type].retry_count = 0;
  cm_state.connections[type].next_retry_delay_ms =
      cm_state.config.initial_retry_delay_ms;

  xSemaphoreGive(cm_state.mutex);

  ESP_LOGI(TAG, "Registered connection: %s", cm_state.connections[type].name);

  return ESP_OK;
}

void connection_manager_update_state(connection_type_t type,
                                     connection_state_t state) {
  if (!cm_state.initialized || type >= CONN_TYPE_MAX) {
    return;
  }

  xSemaphoreTake(cm_state.mutex, portMAX_DELAY);

  connection_entry_t *conn = &cm_state.connections[type];
  connection_state_t old_state = conn->state;

  if (old_state != state) {
    conn->state = state;

    ESP_LOGI(TAG, "[%s] State: %s -> %s",
             conn->name ? conn->name : type_strings[type],
             state_strings[old_state], state_strings[state]);

    if (state == CONN_STATE_CONNECTED) {
      // Reset retry state on successful connection
      conn->retry_count = 0;
      conn->next_retry_delay_ms = cm_state.config.initial_retry_delay_ms;
      conn->reconnect_pending = false;
    } else if (state == CONN_STATE_DISCONNECTED || state == CONN_STATE_ERROR) {
      // Mark for reconnection
      if (conn->registered && conn->reconnect_fn != NULL) {
        conn->reconnect_pending = true;
      }
    }

    // Call status callback
    if (cm_state.status_callback) {
      cm_state.status_callback(type, state, conn->retry_count);
    }
  }

  xSemaphoreGive(cm_state.mutex);
}

connection_state_t connection_manager_get_state(connection_type_t type) {
  if (!cm_state.initialized || type >= CONN_TYPE_MAX) {
    return CONN_STATE_DISCONNECTED;
  }

  return cm_state.connections[type].state;
}

bool connection_manager_all_connected(void) {
  if (!cm_state.initialized) {
    return false;
  }

  for (int i = 0; i < CONN_TYPE_MAX; i++) {
    if (cm_state.connections[i].registered &&
        cm_state.connections[i].state != CONN_STATE_CONNECTED &&
        cm_state.connections[i].state != CONN_STATE_DISABLED) {
      return false;
    }
  }

  return true;
}

esp_err_t connection_manager_request_reconnect(connection_type_t type) {
  if (!cm_state.initialized || type >= CONN_TYPE_MAX) {
    return ESP_ERR_INVALID_STATE;
  }

  xSemaphoreTake(cm_state.mutex, portMAX_DELAY);

  connection_entry_t *conn = &cm_state.connections[type];

  if (!conn->registered || conn->reconnect_fn == NULL) {
    xSemaphoreGive(cm_state.mutex);
    return ESP_ERR_INVALID_STATE;
  }

  conn->reconnect_pending = true;
  conn->next_retry_delay_ms = cm_state.config.initial_retry_delay_ms;
  conn->last_attempt_time = 0; // Allow immediate retry

  xSemaphoreGive(cm_state.mutex);

  ESP_LOGI(TAG, "[%s] Reconnection requested", conn->name);

  return ESP_OK;
}

void connection_manager_register_callback(
    connection_status_callback_t callback) {
  cm_state.status_callback = callback;
  ESP_LOGI(TAG, "Status callback registered");
}

int connection_manager_get_retry_count(connection_type_t type) {
  if (!cm_state.initialized || type >= CONN_TYPE_MAX) {
    return 0;
  }

  return cm_state.connections[type].retry_count;
}

/**
 * @brief Connection manager monitoring task
 */
static void connection_manager_task(void *arg) {
  ESP_LOGI(TAG, "Connection manager task started");

  while (cm_state.running) {
    TickType_t now = xTaskGetTickCount();

    for (int i = 0; i < CONN_TYPE_MAX; i++) {
      connection_entry_t *conn = &cm_state.connections[i];

      if (!conn->registered || !conn->reconnect_pending) {
        continue;
      }

      // Check if it's time to retry
      TickType_t elapsed = now - conn->last_attempt_time;
      if (elapsed < pdMS_TO_TICKS(conn->next_retry_delay_ms)) {
        continue;
      }

      // Check max retry count
      if (cm_state.config.max_retry_count > 0 &&
          conn->retry_count >= cm_state.config.max_retry_count) {
        ESP_LOGW(TAG, "[%s] Max retries (%d) reached, giving up", conn->name,
                 cm_state.config.max_retry_count);
        conn->reconnect_pending = false;
        conn->state = CONN_STATE_ERROR;
        continue;
      }

      // Attempt reconnection
      ESP_LOGI(TAG, "[%s] Attempting reconnection (retry %d, delay %lu ms)",
               conn->name, conn->retry_count + 1, conn->next_retry_delay_ms);

      conn->state = CONN_STATE_CONNECTING;
      conn->last_attempt_time = now;

      if (cm_state.status_callback) {
        cm_state.status_callback((connection_type_t)i, CONN_STATE_CONNECTING,
                                 conn->retry_count);
      }

      // Call reconnection function
      esp_err_t ret = ESP_FAIL;
      if (conn->reconnect_fn) {
        ret = conn->reconnect_fn();
      }

      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "[%s] Reconnection successful!", conn->name);
        conn->state = CONN_STATE_CONNECTED;
        conn->reconnect_pending = false;
        conn->retry_count = 0;
        conn->next_retry_delay_ms = cm_state.config.initial_retry_delay_ms;

        if (cm_state.status_callback) {
          cm_state.status_callback((connection_type_t)i, CONN_STATE_CONNECTED,
                                   0);
        }
      } else {
        ESP_LOGW(TAG, "[%s] Reconnection failed: %s", conn->name,
                 esp_err_to_name(ret));
        conn->state = CONN_STATE_DISCONNECTED;
        conn->retry_count++;

        // Exponential backoff
        conn->next_retry_delay_ms =
            (uint32_t)(conn->next_retry_delay_ms *
                       cm_state.config.backoff_multiplier);
        if (conn->next_retry_delay_ms > cm_state.config.max_retry_delay_ms) {
          conn->next_retry_delay_ms = cm_state.config.max_retry_delay_ms;
        }

        if (cm_state.status_callback) {
          cm_state.status_callback((connection_type_t)i,
                                   CONN_STATE_DISCONNECTED, conn->retry_count);
        }
      }
    }

    // Sleep for health check interval
    vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds
  }

  ESP_LOGI(TAG, "Connection manager task stopped");
  vTaskDelete(NULL);
}

esp_err_t connection_manager_start(void) {
  if (!cm_state.initialized) {
    ESP_LOGE(TAG, "Connection manager not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (cm_state.running) {
    ESP_LOGW(TAG, "Connection manager already running");
    return ESP_OK;
  }

  cm_state.running = true;

  BaseType_t ret = xTaskCreate(connection_manager_task, "conn_mgr", 4096, NULL,
                               3, &cm_state.task_handle);

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create connection manager task");
    cm_state.running = false;
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Connection manager started");
  return ESP_OK;
}

void connection_manager_stop(void) {
  if (!cm_state.running) {
    return;
  }

  cm_state.running = false;

  // Wait for task to stop
  vTaskDelay(pdMS_TO_TICKS(1000));

  cm_state.task_handle = NULL;

  ESP_LOGI(TAG, "Connection manager stopped");
}

void connection_manager_deinit(void) {
  connection_manager_stop();

  if (cm_state.mutex) {
    vSemaphoreDelete(cm_state.mutex);
    cm_state.mutex = NULL;
  }

  cm_state.initialized = false;

  ESP_LOGI(TAG, "Connection manager deinitialized");
}

const char *connection_manager_type_to_string(connection_type_t type) {
  if (type >= CONN_TYPE_MAX) {
    return "UNKNOWN";
  }
  return type_strings[type];
}

const char *connection_manager_state_to_string(connection_state_t state) {
  if (state > CONN_STATE_DISABLED) {
    return "UNKNOWN";
  }
  return state_strings[state];
}
