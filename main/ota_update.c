/**
 * @file ota_update.c
 * @brief OTA (Over-The-Air) firmware update implementation
 */

#include "ota_update.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_app_format.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ota_update";

// OTA state
static ota_state_t ota_state = OTA_STATE_IDLE;
static int ota_progress = 0;
static bool ota_running = false;
static ota_progress_callback_t progress_callback = NULL;

// Task handle
static TaskHandle_t ota_task_handle = NULL;

/**
 * @brief Notify progress callback
 */
static void notify_progress(ota_state_t state, int progress, const char *message)
{
    ota_state = state;
    ota_progress = progress;

    if (progress_callback) {
        progress_callback(state, progress, message);
    }

    ESP_LOGI(TAG, "[%d%%] %s", progress, message);
}

/**
 * @brief HTTP event handler for OTA
 */
static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP error");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "Connected to server");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "Headers sent");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "Header: %s: %s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "Received %d bytes", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP transfer finished");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from server");
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief OTA update task
 */
static void ota_update_task(void *pvParameter)
{
    const char *url = (const char *)pvParameter;
    esp_err_t ret;

    ESP_LOGI(TAG, "Starting OTA update from: %s", url);
    notify_progress(OTA_STATE_DOWNLOADING, 0, "Starting OTA update");

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = ota_http_event_handler,
        .keep_alive_enable = true,
        .timeout_ms = 30000,
    };

    // Configure OTA
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    ret = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(ret));
        notify_progress(OTA_STATE_FAILED, 0, "Failed to start OTA");
        goto ota_end;
    }

    // Get image size
    int image_size = esp_https_ota_get_image_size(https_ota_handle);
    ESP_LOGI(TAG, "Image size: %d bytes", image_size);

    // Download and write firmware
    int downloaded = 0;
    while (1) {
        ret = esp_https_ota_perform(https_ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        // Update progress
        downloaded = esp_https_ota_get_image_len_read(https_ota_handle);
        if (image_size > 0) {
            int progress = (downloaded * 100) / image_size;
            char msg[64];
            snprintf(msg, sizeof(msg), "Downloading: %d/%d bytes", downloaded, image_size);
            notify_progress(OTA_STATE_DOWNLOADING, progress, msg);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Check if download completed successfully
    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        ESP_LOGE(TAG, "Complete data was not received");
        notify_progress(OTA_STATE_FAILED, ota_progress, "Incomplete download");
        ret = ESP_FAIL;
    } else {
        notify_progress(OTA_STATE_VERIFYING, 100, "Verifying firmware");

        ret = esp_https_ota_finish(https_ota_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA update successful!");
            notify_progress(OTA_STATE_SUCCESS, 100, "Update successful - Rebooting...");

            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        } else {
            if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
                notify_progress(OTA_STATE_FAILED, 100, "Image validation failed");
            } else {
                ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(ret));
                notify_progress(OTA_STATE_FAILED, 100, "Update failed");
            }
        }
        https_ota_handle = NULL;
    }

ota_end:
    if (https_ota_handle) {
        esp_https_ota_abort(https_ota_handle);
    }

    // Free URL string
    free((void *)url);

    ota_running = false;
    ota_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Initialize OTA update module
 */
esp_err_t ota_update_init(void)
{
    ESP_LOGI(TAG, "OTA update module initialized");
    ESP_LOGI(TAG, "Current version: %s", ota_update_get_current_version());

    // Check if we rolled back from a failed update
    if (ota_update_check_rollback()) {
        ESP_LOGW(TAG, "Device rolled back from failed OTA update");
    }

    return ESP_OK;
}

/**
 * @brief Start OTA update from HTTP URL
 */
esp_err_t ota_update_start(const char *url)
{
    if (ota_running) {
        ESP_LOGW(TAG, "OTA update already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (!url || strlen(url) == 0) {
        ESP_LOGE(TAG, "Invalid URL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting OTA update task");

    // Duplicate URL string (task will free it)
    char *url_copy = strdup(url);
    if (!url_copy) {
        ESP_LOGE(TAG, "Failed to allocate URL string");
        return ESP_ERR_NO_MEM;
    }

    ota_running = true;
    ota_state = OTA_STATE_IDLE;
    ota_progress = 0;

    // Create OTA task
    BaseType_t ret = xTaskCreate(
        ota_update_task,
        "ota_update_task",
        8192,
        (void *)url_copy,
        5,
        &ota_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        free(url_copy);
        ota_running = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Check if OTA update is in progress
 */
bool ota_update_is_running(void)
{
    return ota_running;
}

/**
 * @brief Get current OTA state
 */
ota_state_t ota_update_get_state(void)
{
    return ota_state;
}

/**
 * @brief Get current OTA progress
 */
int ota_update_get_progress(void)
{
    return ota_progress;
}

/**
 * @brief Register progress callback
 */
void ota_update_register_callback(ota_progress_callback_t callback)
{
    progress_callback = callback;
    ESP_LOGI(TAG, "Progress callback registered");
}

/**
 * @brief Get current firmware version
 */
const char* ota_update_get_current_version(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    return app_desc->version;
}

/**
 * @brief Check if partition was rolled back
 */
bool ota_update_check_rollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "Running partition is in pending verify state");
            return true;
        }
    }

    return false;
}

/**
 * @brief Mark current partition as valid
 */
esp_err_t ota_update_mark_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "Marking current partition as valid");
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to mark partition valid: %s", esp_err_to_name(err));
                return err;
            }
            ESP_LOGI(TAG, "Current partition marked as valid");
        }
    }

    return ESP_OK;
}
