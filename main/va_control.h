/**
 * @file va_control.h
 * @brief Small control/status API for the Voice Assistant (used by the web UI).
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Read-only status / config getters
float va_control_get_wwd_threshold(void);
uint32_t va_control_get_vad_threshold(void);
uint32_t va_control_get_vad_silence_duration_ms(void);
uint32_t va_control_get_vad_min_speech_ms(void);
uint32_t va_control_get_vad_max_recording_ms(void);

bool va_control_get_agc_enabled(void);
uint16_t va_control_get_agc_target_level(void);

bool va_control_get_pipeline_active(void);
bool va_control_get_wwd_running(void);

// Config setters
esp_err_t va_control_set_wwd_threshold(float threshold);
esp_err_t va_control_set_vad_threshold(uint32_t threshold);
esp_err_t va_control_set_vad_silence_duration_ms(uint32_t ms);
esp_err_t va_control_set_vad_min_speech_ms(uint32_t ms);
esp_err_t va_control_set_vad_max_recording_ms(uint32_t ms);

esp_err_t va_control_set_agc_enabled(bool enabled);
esp_err_t va_control_set_agc_target_level(uint16_t target_level);

// Actions
void va_control_action_restart(void);
void va_control_action_wwd_resume(void);
void va_control_action_wwd_stop(void);
void va_control_action_test_tts(const char *text);

#ifdef __cplusplus
}
#endif

