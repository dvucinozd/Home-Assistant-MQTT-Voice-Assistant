#include "wwd.h"
#include "audio_capture.h"
#include "esp_log.h"

static const char *TAG = "wwd";
static float current_threshold = 0.5f; // Default

esp_err_t wwd_set_threshold(float threshold) {
    if (threshold < 0.0f || threshold > 1.0f) {
        ESP_LOGE(TAG, "Invalid threshold: %f", threshold);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Setting WWD threshold to %.2f", threshold);

    // Call into audio_capture to update the AFE
    esp_err_t ret = audio_capture_set_wakenet_threshold(threshold);
    if (ret == ESP_OK) {
        current_threshold = threshold;
    } else {
        ESP_LOGE(TAG, "Failed to set WWD threshold in audio_capture");
    }

    return ret;
}

float wwd_get_threshold(void) {
    return current_threshold;
}
