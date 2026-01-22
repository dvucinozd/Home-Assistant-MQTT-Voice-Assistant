/**
 * @file timer_manager.c
 * @brief Multi-timer manager implementation
 */

#include "timer_manager.h"
#include "beep_tone.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mqtt_ha.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "timer_mgr";

// Timer entries
static timer_entry_t timers[TIMER_MAX_COUNT];
static SemaphoreHandle_t timer_mutex = NULL;
static TaskHandle_t timer_task_handle = NULL;
static timer_expired_callback_t expired_callback = NULL;
static uint8_t next_timer_id = 1;

// Warning beep thresholds
#define WARNING_THRESHOLD_SEC 120 // Start warning at 2 minutes
#define WARNING_INTERVAL_SEC 30   // Beep every 30 seconds

static void timer_tick_task(void *arg);
static void check_warning_beeps(timer_entry_t *timer);

// =============================================================================
// PUBLIC API
// =============================================================================

esp_err_t timer_manager_init(timer_expired_callback_t callback) {
  ESP_LOGI(TAG, "Initializing Timer Manager (max %d timers)", TIMER_MAX_COUNT);

  expired_callback = callback;

  // Clear all timers
  memset(timers, 0, sizeof(timers));

  // Create mutex
  if (timer_mutex == NULL) {
    timer_mutex = xSemaphoreCreateMutex();
    if (timer_mutex == NULL) {
      ESP_LOGE(TAG, "Failed to create mutex");
      return ESP_ERR_NO_MEM;
    }
  }

  // Start tick task (counts down every second)
  if (timer_task_handle == NULL) {
    BaseType_t ret = xTaskCreate(timer_tick_task, "timer_tick", 3072, NULL, 5,
                                 &timer_task_handle);
    if (ret != pdPASS) {
      ESP_LOGE(TAG, "Failed to create timer task");
      return ESP_ERR_NO_MEM;
    }
  }

  return ESP_OK;
}

esp_err_t timer_manager_start(uint32_t seconds, const char *label,
                              uint8_t *out_id) {
  if (seconds == 0) {
    ESP_LOGW(TAG, "Cannot start timer with 0 seconds");
    return ESP_ERR_INVALID_ARG;
  }

  xSemaphoreTake(timer_mutex, portMAX_DELAY);

  // Find empty slot
  int slot = -1;
  for (int i = 0; i < TIMER_MAX_COUNT; i++) {
    if (timers[i].state == TIMER_STATE_IDLE) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    xSemaphoreGive(timer_mutex);
    ESP_LOGW(TAG, "Max timers (%d) reached", TIMER_MAX_COUNT);
    return ESP_ERR_NO_MEM;
  }

  // Assign ID (wrap at 255)
  uint8_t id = next_timer_id++;
  if (next_timer_id == 0)
    next_timer_id = 1;

  // Setup timer
  timers[slot].id = id;
  timers[slot].duration_seconds = seconds;
  timers[slot].remaining_seconds = seconds;
  timers[slot].state = TIMER_STATE_RUNNING;

  if (label && label[0]) {
    strncpy(timers[slot].label, label, TIMER_LABEL_LEN - 1);
    timers[slot].label[TIMER_LABEL_LEN - 1] = '\0';
  } else {
    snprintf(timers[slot].label, TIMER_LABEL_LEN, "Timer %d", id);
  }

  if (out_id) {
    *out_id = id;
  }

  xSemaphoreGive(timer_mutex);

  // Format time for log
  char time_str[16];
  timer_manager_format_time(seconds, time_str, sizeof(time_str));
  ESP_LOGI(TAG, "Timer #%d started: %s (%lu seconds)", id, time_str,
           (unsigned long)seconds);

  // Publish MQTT state
  timer_manager_publish_mqtt_state();

  return ESP_OK;
}

esp_err_t timer_manager_stop(uint8_t timer_id) {
  if (timer_id == 0) {
    timer_manager_stop_all();
    return ESP_OK;
  }

  xSemaphoreTake(timer_mutex, portMAX_DELAY);

  bool found = false;
  for (int i = 0; i < TIMER_MAX_COUNT; i++) {
    if (timers[i].id == timer_id && timers[i].state == TIMER_STATE_RUNNING) {
      timers[i].state = TIMER_STATE_IDLE;
      timers[i].remaining_seconds = 0;
      found = true;
      ESP_LOGI(TAG, "Timer #%d stopped", timer_id);
      break;
    }
  }

  xSemaphoreGive(timer_mutex);

  if (found) {
    timer_manager_publish_mqtt_state();
  }

  return found ? ESP_OK : ESP_ERR_NOT_FOUND;
}

void timer_manager_stop_all(void) {
  xSemaphoreTake(timer_mutex, portMAX_DELAY);

  for (int i = 0; i < TIMER_MAX_COUNT; i++) {
    if (timers[i].state == TIMER_STATE_RUNNING) {
      timers[i].state = TIMER_STATE_IDLE;
      timers[i].remaining_seconds = 0;
    }
  }

  xSemaphoreGive(timer_mutex);

  ESP_LOGI(TAG, "All timers stopped");
  timer_manager_publish_mqtt_state();
}

uint32_t timer_manager_get_remaining(uint8_t timer_id) {
  xSemaphoreTake(timer_mutex, portMAX_DELAY);

  uint32_t remaining = 0;
  for (int i = 0; i < TIMER_MAX_COUNT; i++) {
    if (timers[i].id == timer_id && timers[i].state == TIMER_STATE_RUNNING) {
      remaining = timers[i].remaining_seconds;
      break;
    }
  }

  xSemaphoreGive(timer_mutex);
  return remaining;
}

int timer_manager_get_active_count(void) {
  xSemaphoreTake(timer_mutex, portMAX_DELAY);

  int count = 0;
  for (int i = 0; i < TIMER_MAX_COUNT; i++) {
    if (timers[i].state == TIMER_STATE_RUNNING) {
      count++;
    }
  }

  xSemaphoreGive(timer_mutex);
  return count;
}

bool timer_manager_get_next_expiring(timer_entry_t *out_entry) {
  if (!out_entry)
    return false;

  xSemaphoreTake(timer_mutex, portMAX_DELAY);

  bool found = false;
  uint32_t min_remaining = UINT32_MAX;

  for (int i = 0; i < TIMER_MAX_COUNT; i++) {
    if (timers[i].state == TIMER_STATE_RUNNING) {
      if (timers[i].remaining_seconds < min_remaining) {
        min_remaining = timers[i].remaining_seconds;
        *out_entry = timers[i];
        found = true;
      }
    }
  }

  xSemaphoreGive(timer_mutex);
  return found;
}

void timer_manager_format_time(uint32_t seconds, char *out_str,
                               size_t out_len) {
  if (!out_str || out_len < 6)
    return;

  uint32_t hours = seconds / 3600;
  uint32_t mins = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;

  if (hours > 0) {
    snprintf(out_str, out_len, "%u:%02u:%02u", (unsigned)hours, (unsigned)mins,
             (unsigned)secs);
  } else {
    snprintf(out_str, out_len, "%02u:%02u", (unsigned)mins, (unsigned)secs);
  }
}

bool timer_manager_is_active(void) {
  return timer_manager_get_active_count() > 0;
}

void timer_manager_publish_mqtt_state(void) {
  if (!mqtt_ha_is_connected())
    return;

  int count = timer_manager_get_active_count();

  // Publish count
  char count_str[8];
  snprintf(count_str, sizeof(count_str), "%d", count);
  mqtt_ha_update_sensor("timer_count", count_str);

  // Publish active state
  mqtt_ha_update_sensor("timer_active", count > 0 ? "true" : "false");

  // Publish remaining time of next expiring timer
  timer_entry_t next;
  if (timer_manager_get_next_expiring(&next)) {
    char time_str[16];
    timer_manager_format_time(next.remaining_seconds, time_str,
                              sizeof(time_str));
    mqtt_ha_update_sensor("timer_remaining", time_str);
  } else {
    mqtt_ha_update_sensor("timer_remaining", "--:--");
  }
}

// =============================================================================
// INTERNAL TICK TASK
// =============================================================================

static void check_warning_beeps(timer_entry_t *timer) {
  // Only beep if within warning threshold
  if (timer->remaining_seconds > WARNING_THRESHOLD_SEC)
    return;
  if (timer->remaining_seconds == 0)
    return;

  // Beep at warning interval boundaries
  if (timer->remaining_seconds % WARNING_INTERVAL_SEC == 0) {
    ESP_LOGI(TAG, "Timer #%d warning: %lu seconds remaining", timer->id,
             (unsigned long)timer->remaining_seconds);
    beep_tone_play(800, 100, 40); // Short warning beep
  }
}

static void timer_tick_task(void *arg) {
  (void)arg;

  TickType_t last_wake = xTaskGetTickCount();
  uint8_t mqtt_publish_counter = 0;

  while (1) {
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000)); // Tick every second

    xSemaphoreTake(timer_mutex, portMAX_DELAY);

    bool any_expired = false;
    uint8_t expired_ids[TIMER_MAX_COUNT];
    int expired_count = 0;

    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
      if (timers[i].state == TIMER_STATE_RUNNING) {
        if (timers[i].remaining_seconds > 0) {
          timers[i].remaining_seconds--;

          // Check for warning beeps
          check_warning_beeps(&timers[i]);
        }

        if (timers[i].remaining_seconds == 0) {
          timers[i].state = TIMER_STATE_EXPIRED;
          expired_ids[expired_count++] = timers[i].id;
          any_expired = true;
          ESP_LOGI(TAG, "Timer #%d expired!", timers[i].id);
        }
      }

      // Clean up expired timers after callback
      if (timers[i].state == TIMER_STATE_EXPIRED) {
        timers[i].state = TIMER_STATE_IDLE;
      }
    }

    xSemaphoreGive(timer_mutex);

    // Call expired callbacks outside mutex
    for (int i = 0; i < expired_count; i++) {
      if (expired_callback) {
        expired_callback(expired_ids[i]);
      }
    }

    // Publish MQTT state every 5 seconds (or immediately on expiry)
    mqtt_publish_counter++;
    if (any_expired || mqtt_publish_counter >= 5) {
      mqtt_publish_counter = 0;
      timer_manager_publish_mqtt_state();
    }
  }
}
