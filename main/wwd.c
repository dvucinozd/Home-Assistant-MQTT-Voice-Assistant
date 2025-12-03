/**
 * Wake Word Detection (WWD) Module Implementation
 *
 * Uses ESP-SR WakeNet9l for continuous wake word monitoring.
 */

#include "wwd.h"
#include "esp_log.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "model_path.h"
#include <string.h>

static const char *TAG = "wwd";

// Wake word detection state
static struct {
    bool initialized;
    bool running;
    wwd_config_t config;
    esp_wn_iface_t *wakenet;
    model_iface_data_t *model_data;
} wwd_state = {
    .initialized = false,
    .running = false,
    .wakenet = NULL,
    .model_data = NULL
};

wwd_config_t wwd_get_default_config(void)
{
    wwd_config_t config = {
        .sample_rate = 16000,
        .bit_width = 16,
        .channels = 1,
        .detection_threshold = 0.5f,
        .callback = NULL,
        .user_data = NULL
    };
    return config;
}

esp_err_t wwd_init(const wwd_config_t *config)
{
    if (wwd_state.initialized) {
        ESP_LOGW(TAG, "WWD already initialized");
        return ESP_OK;
    }

    if (!config || !config->callback) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing Wake Word Detection...");
    ESP_LOGI(TAG, "Sample rate: %d Hz, Channels: %d, Threshold: %.2f",
             config->sample_rate, config->channels, config->detection_threshold);

    // Copy configuration
    memcpy(&wwd_state.config, config, sizeof(wwd_config_t));

    // Step 1: Initialize models from flash (managed_components)
    ESP_LOGI(TAG, "Loading models from flash partition...");
    srmodel_list_t *models = esp_srmodel_init("model");  // Load from "model" partition
    if (!models) {
        ESP_LOGE(TAG, "Failed to load models from flash partition");
        ESP_LOGE(TAG, "Trying to load models from component directory...");
        // Fallback: Try NULL to load from component directory
        models = esp_srmodel_init(NULL);
        if (!models) {
            ESP_LOGE(TAG, "Failed to load models from component directory");
            return ESP_FAIL;
        }
    }

    // Step 2: Find WakeNet model - directly access first model
    // Note: esp_srmodel_filter has a bug - it requires both keywords to be non-NULL
    // So we manually select the first available model from the list
    char *model_name = NULL;
    if (models->num > 0) {
        model_name = models->model_name[0];
        ESP_LOGI(TAG, "Found %d models, using first: %s", models->num, model_name);
    } else {
        ESP_LOGE(TAG, "No models found in flash - models->num = 0");
        return ESP_FAIL;
    }

    if (!model_name) {
        ESP_LOGE(TAG, "Model name is NULL");
        return ESP_FAIL;
    }

    // Step 3: Get WakeNet interface
    const esp_wn_iface_t *wakenet = esp_wn_handle_from_name(model_name);
    if (!wakenet) {
        ESP_LOGE(TAG, "Failed to get WakeNet interface for %s", model_name);
        return ESP_FAIL;
    }
    wwd_state.wakenet = (esp_wn_iface_t *)wakenet;

    ESP_LOGI(TAG, "WakeNet interface obtained");

    // Step 4: Determine detection mode based on threshold
    det_mode_t det_mode = (config->detection_threshold >= 0.95f) ? DET_MODE_95 : DET_MODE_90;

    // Step 5: Create model data with model name and detection mode
    wwd_state.model_data = wwd_state.wakenet->create(model_name, det_mode);

    if (!wwd_state.model_data) {
        ESP_LOGE(TAG, "Failed to create WakeNet model");
        return ESP_FAIL;
    }

    // Set custom detection threshold if specified
    if (config->detection_threshold > 0.0f && config->detection_threshold < 1.0f) {
        int num_words = wwd_state.wakenet->get_word_num(wwd_state.model_data);
        for (int i = 1; i <= num_words; i++) {
            wwd_state.wakenet->set_det_threshold(wwd_state.model_data, config->detection_threshold, i);
            ESP_LOGI(TAG, "Set detection threshold %.2f for word %d", config->detection_threshold, i);
        }
    }

    wwd_state.initialized = true;

    // Log wake word info
    int num_words = wwd_state.wakenet->get_word_num(wwd_state.model_data);
    ESP_LOGI(TAG, "Wake Word Detection initialized successfully");
    ESP_LOGI(TAG, "Model: wn9, Mode: %s, Words: %d",
             (det_mode == DET_MODE_95) ? "Aggressive(95)" : "Normal(90)", num_words);

    for (int i = 1; i <= num_words; i++) {
        char *word_name = wwd_state.wakenet->get_word_name(wwd_state.model_data, i);
        ESP_LOGI(TAG, "  Word %d: %s", i, word_name ? word_name : "unknown");
    }

    return ESP_OK;
}

esp_err_t wwd_start(void)
{
    if (!wwd_state.initialized) {
        ESP_LOGE(TAG, "WWD not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (wwd_state.running) {
        ESP_LOGW(TAG, "WWD already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting wake word detection...");
    wwd_state.running = true;

    return ESP_OK;
}

esp_err_t wwd_stop(void)
{
    if (!wwd_state.running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping wake word detection");
    wwd_state.running = false;

    return ESP_OK;
}

esp_err_t wwd_feed_audio(const int16_t *audio_data, size_t length)
{
    if (!wwd_state.running) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!audio_data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Feed audio to WakeNet
    wakenet_state_t state = wwd_state.wakenet->detect(
        wwd_state.model_data,
        (int16_t *)audio_data
    );

    // Check if wake word detected
    if (state == WAKENET_DETECTED) {
        ESP_LOGI(TAG, "🎤 Wake word detected!");

        // Trigger callback
        if (wwd_state.config.callback) {
            wwd_state.config.callback(WWD_EVENT_DETECTED, wwd_state.config.user_data);
        }

        // Stop after detection (will restart after TTS finishes)
        wwd_state.running = false;
    }

    return ESP_OK;
}

bool wwd_is_running(void)
{
    return wwd_state.running;
}

esp_err_t wwd_deinit(void)
{
    if (!wwd_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing wake word detection");

    // Stop if running
    wwd_stop();

    // Destroy model
    if (wwd_state.model_data && wwd_state.wakenet) {
        wwd_state.wakenet->destroy(wwd_state.model_data);
        wwd_state.model_data = NULL;
    }

    wwd_state.wakenet = NULL;
    wwd_state.initialized = false;

    ESP_LOGI(TAG, "Wake word detection deinitialized");

    return ESP_OK;
}
