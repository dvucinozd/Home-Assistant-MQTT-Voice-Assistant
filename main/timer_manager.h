/**
 * @file timer_manager.h
 * @brief Multi-timer manager for voice assistant
 *
 * Supports up to 3 concurrent timers with:
 * - Countdown tracking
 * - TTS voice confirmation
 * - Periodic beep warnings
 * - MQTT state publishing
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIMER_MAX_COUNT 3
#define TIMER_LABEL_LEN 32

typedef enum {
  TIMER_STATE_IDLE = 0,
  TIMER_STATE_RUNNING,
  TIMER_STATE_EXPIRED
} timer_state_t;

typedef struct {
  uint8_t id;                 // Timer ID (1-based)
  uint32_t duration_seconds;  // Original duration
  uint32_t remaining_seconds; // Countdown remaining
  timer_state_t state;
  char label[TIMER_LABEL_LEN]; // Optional label
} timer_entry_t;

/**
 * @brief Callback when a timer expires
 * @param timer_id ID of expired timer
 */
typedef void (*timer_expired_callback_t)(uint8_t timer_id);

/**
 * @brief Initialize timer manager
 * @param callback Function to call when timer expires
 * @return ESP_OK on success
 */
esp_err_t timer_manager_init(timer_expired_callback_t callback);

/**
 * @brief Start a new timer
 * @param seconds Duration in seconds
 * @param label Optional label (can be NULL)
 * @param out_id Pointer to store assigned timer ID
 * @return ESP_OK on success, ESP_ERR_NO_MEM if max timers reached
 */
esp_err_t timer_manager_start(uint32_t seconds, const char *label,
                              uint8_t *out_id);

/**
 * @brief Stop a specific timer
 * @param timer_id ID of timer to stop (0 = stop all)
 * @return ESP_OK on success
 */
esp_err_t timer_manager_stop(uint8_t timer_id);

/**
 * @brief Stop all timers
 */
void timer_manager_stop_all(void);

/**
 * @brief Get remaining time for a timer
 * @param timer_id Timer ID
 * @return Remaining seconds, or 0 if not found/expired
 */
uint32_t timer_manager_get_remaining(uint8_t timer_id);

/**
 * @brief Get count of active timers
 * @return Number of running timers
 */
int timer_manager_get_active_count(void);

/**
 * @brief Get the timer with least remaining time
 * @param out_entry Pointer to store timer entry
 * @return true if an active timer exists
 */
bool timer_manager_get_next_expiring(timer_entry_t *out_entry);

/**
 * @brief Format remaining time as string (MM:SS or HH:MM:SS)
 * @param seconds Seconds to format
 * @param out_str Output buffer
 * @param out_len Output buffer length
 */
void timer_manager_format_time(uint32_t seconds, char *out_str, size_t out_len);

/**
 * @brief Check if any timer is active
 * @return true if at least one timer is running
 */
bool timer_manager_is_active(void);

/**
 * @brief Publish timer states to MQTT
 */
void timer_manager_publish_mqtt_state(void);

#ifdef __cplusplus
}
#endif
