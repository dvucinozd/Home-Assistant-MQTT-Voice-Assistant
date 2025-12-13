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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "sdkconfig.h"

static const char *TAG = "wwd";

static const char *select_model_name(const srmodel_list_t *models) {
    if (models == NULL || models->num <= 0 || models->model_name == NULL) {
        return NULL;
    }

    // Prefer common "Hi ESP" model if present
    for (int i = 0; i < models->num; i++) {
        const char *name = models->model_name[i];
        if (name && strcmp(name, "wn9_hiesp") == 0) {
            return name;
        }
    }

    // Otherwise prefer any WakeNet9 model
    for (int i = 0; i < models->num; i++) {
        const char *name = models->model_name[i];
        if (name && strncmp(name, "wn9_", 4) == 0) {
            return name;
        }
    }

    // Otherwise any WakeNet model
    for (int i = 0; i < models->num; i++) {
        const char *name = models->model_name[i];
        if (name && strncmp(name, "wn", 2) == 0) {
            return name;
        }
    }

    return models->model_name[0];
}

// Wake word detection state
static struct {
    bool initialized;
    bool running;
    wwd_config_t config;
    esp_wn_iface_t *wakenet;
    model_iface_data_t *model_data;
    int chunk_size;
    int16_t *chunk_buffer;
    size_t chunk_filled;
} wwd_state = {
    .initialized = false,
    .running = false,
    .wakenet = NULL,
    .model_data = NULL,
    .chunk_size = 0,
    .chunk_buffer = NULL,
    .chunk_filled = 0
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

    // Step 1: Initialize models
    srmodel_list_t *models = NULL;
#ifdef CONFIG_MODEL_IN_SDCARD
    // IMPORTANT: esp_srmodel_init() caches the first scan results globally.
    // If we call it before SD is mounted, it can cache an empty model list and
    // never rescan, so WakeNet will fail forever. Guard with an opendir() check.
    ESP_LOGI(TAG, "Loading models from SD card path: /sdcard/srmodels");
    DIR *dir = opendir("/sdcard/srmodels");
    if (dir == NULL) {
        ESP_LOGW(TAG, "SD model path not available yet (/sdcard/srmodels); retry after SD mount");
        return ESP_ERR_INVALID_STATE;
    }
    closedir(dir);
    models = esp_srmodel_init("/sdcard/srmodels");
#else
    ESP_LOGI(TAG, "Loading models from flash partition: model");
    models = esp_srmodel_init("model");  // Load from "model" partition
#endif
    if (!models) {
        ESP_LOGE(TAG, "Failed to load models");
        return ESP_FAIL;
    }

    // Step 2: Select a WakeNet model deterministically
    const char *model_name = select_model_name(models);
    if (model_name == NULL) {
        ESP_LOGE(TAG, "No models found (models->num = %d)", models ? models->num : -1);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Found %d models, selected: %s", models->num, model_name);

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

    // Query expected chunk size for detect() and allocate staging buffer.
    wwd_state.chunk_size = wwd_state.wakenet->get_samp_chunksize(wwd_state.model_data);
    if (wwd_state.chunk_size <= 0) {
        ESP_LOGE(TAG, "Invalid WakeNet chunk size: %d", wwd_state.chunk_size);
        wwd_state.wakenet->destroy(wwd_state.model_data);
        wwd_state.model_data = NULL;
        return ESP_FAIL;
    }

    wwd_state.chunk_buffer = (int16_t *)malloc((size_t)wwd_state.chunk_size * sizeof(int16_t));
    if (wwd_state.chunk_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate WakeNet chunk buffer (%d samples)", wwd_state.chunk_size);
        wwd_state.wakenet->destroy(wwd_state.model_data);
        wwd_state.model_data = NULL;
        return ESP_ERR_NO_MEM;
    }
    wwd_state.chunk_filled = 0;

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
    ESP_LOGI(TAG, "WakeNet expected chunk size: %d samples", wwd_state.chunk_size);
    ESP_LOGI(TAG, "WakeNet expected sample rate: %d Hz",
             wwd_state.wakenet->get_samp_rate(wwd_state.model_data));

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
    // Some WakeNet library builds crash on clean() before first detect().
    // Rely on a fresh model instance + chunk buffer reset instead.
    wwd_state.chunk_filled = 0;
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

    if (wwd_state.chunk_buffer == NULL || wwd_state.chunk_size <= 0) {
        ESP_LOGW(TAG, "WWD not ready (chunk buffer missing)");
        return ESP_ERR_INVALID_STATE;
    }

    // WakeNet detect() expects exactly get_samp_chunksize() samples per call.
    // Accumulate incoming samples and call detect() in fixed-size chunks.
    size_t remaining = length;
    const int16_t *read_ptr = audio_data;

    while (remaining > 0 && wwd_state.running) {
        size_t space = (size_t)wwd_state.chunk_size - wwd_state.chunk_filled;
        size_t to_copy = (remaining < space) ? remaining : space;

        memcpy(&wwd_state.chunk_buffer[wwd_state.chunk_filled], read_ptr,
               to_copy * sizeof(int16_t));
        wwd_state.chunk_filled += to_copy;
        read_ptr += to_copy;
        remaining -= to_copy;

        if (wwd_state.chunk_filled == (size_t)wwd_state.chunk_size) {
            wakenet_state_t state = wwd_state.wakenet->detect(
                wwd_state.model_data,
                wwd_state.chunk_buffer
            );
            wwd_state.chunk_filled = 0;

            if (state == WAKENET_DETECTED) {
                ESP_LOGI(TAG, "🎤 Wake word detected!");

                if (wwd_state.config.callback) {
                    wwd_state.config.callback(WWD_EVENT_DETECTED, wwd_state.config.user_data);
                }

                // Stop after detection (will restart after pipeline finishes)
                wwd_state.running = false;
            }
        }
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

    if (wwd_state.chunk_buffer) {
        free(wwd_state.chunk_buffer);
        wwd_state.chunk_buffer = NULL;
    }
    wwd_state.chunk_filled = 0;
    wwd_state.chunk_size = 0;

    wwd_state.wakenet = NULL;
    wwd_state.initialized = false;

    ESP_LOGI(TAG, "Wake word detection deinitialized");

    return ESP_OK;
}
