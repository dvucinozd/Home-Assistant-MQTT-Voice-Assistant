#include "va_control.h"
#include "voice_pipeline.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "va_control";

// Getters
float va_control_get_wwd_threshold(void) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    return cfg.wwd_threshold;
}

uint32_t va_control_get_vad_threshold(void) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    return cfg.vad_speech_threshold;
}

uint32_t va_control_get_vad_silence_duration_ms(void) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    return cfg.vad_silence_ms;
}

uint32_t va_control_get_vad_min_speech_ms(void) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    return cfg.vad_min_speech_ms;
}

uint32_t va_control_get_vad_max_recording_ms(void) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    return cfg.vad_max_recording_ms;
}

bool va_control_get_agc_enabled(void) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    return cfg.agc_enabled;
}

uint16_t va_control_get_agc_target_level(void) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    return cfg.agc_target_level;
}

bool va_control_get_pipeline_active(void) {
    return voice_pipeline_is_active();
}

bool va_control_get_wwd_running(void) {
    return voice_pipeline_is_running();
}

// Setters
esp_err_t va_control_set_wwd_threshold(float threshold) {
    ESP_LOGI(TAG, "Setting WWD threshold: %.2f", threshold);
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    cfg.wwd_threshold = threshold;
    return voice_pipeline_update_config(&cfg);
}

esp_err_t va_control_set_vad_threshold(uint32_t threshold) {
    ESP_LOGI(TAG, "Setting VAD threshold: %lu", threshold);
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    cfg.vad_speech_threshold = threshold;
    return voice_pipeline_update_config(&cfg);
}

esp_err_t va_control_set_vad_silence_duration_ms(uint32_t ms) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    cfg.vad_silence_ms = ms;
    return voice_pipeline_update_config(&cfg);
}

esp_err_t va_control_set_vad_min_speech_ms(uint32_t ms) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    cfg.vad_min_speech_ms = ms;
    return voice_pipeline_update_config(&cfg);
}

esp_err_t va_control_set_vad_max_recording_ms(uint32_t ms) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    cfg.vad_max_recording_ms = ms;
    return voice_pipeline_update_config(&cfg);
}

esp_err_t va_control_set_agc_enabled(bool enabled) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    cfg.agc_enabled = enabled;
    return voice_pipeline_update_config(&cfg);
}

esp_err_t va_control_set_agc_target_level(uint16_t target_level) {
    voice_pipeline_config_t cfg;
    voice_pipeline_get_config(&cfg);
    cfg.agc_target_level = target_level;
    return voice_pipeline_update_config(&cfg);
}

// Actions
void va_control_action_restart(void) {
    voice_pipeline_trigger_restart();
}

void va_control_action_wwd_resume(void) {
    voice_pipeline_start();
}

void va_control_action_wwd_stop(void) {
    voice_pipeline_stop();
}

void va_control_action_test_tts(const char *text) {
    voice_pipeline_test_tts(text);
}
