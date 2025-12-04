/**
 * @file webserial.c
 * @brief WebSerial implementation - Remote serial console
 */

#include "webserial.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "webserial";

// HTTP server handle
static httpd_handle_t server = NULL;
static bool server_running = false;

// Log buffer for console viewing
#define LOG_BUFFER_SIZE 8192
static char log_buffer[LOG_BUFFER_SIZE];
static size_t log_buffer_pos = 0;
static SemaphoreHandle_t log_mutex = NULL;

// Original log function pointer
static vprintf_like_t original_log_func = NULL;

// Client tracking (simple counter)
static int client_count = 0;

/**
 * @brief HTML page for WebSerial interface
 */
static const char *webserial_html =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32-P4 WebSerial</title>"
"<style>"
"body{font-family:monospace;margin:0;padding:10px;background:#1e1e1e;color:#d4d4d4}"
"#console{background:#000;color:#0f0;padding:10px;height:80vh;overflow-y:auto;border:1px solid #333;white-space:pre-wrap;word-wrap:break-word}"
".status{padding:5px;margin-bottom:5px;background:#2d2d2d;border-left:3px solid #007acc}"
"button{padding:5px 15px;margin:5px;background:#007acc;color:#fff;border:none;cursor:pointer}"
"button:hover{background:#005a9e}"
"</style>"
"</head>"
"<body>"
"<h2>ESP32-P4 Voice Assistant - WebSerial Console</h2>"
"<div class='status'>Auto-refresh every 2 seconds. Click Refresh for manual update.</div>"
"<div id='console'></div>"
"<div style='margin-top:10px'>"
"<button onclick='refresh()'>Refresh Now</button>"
"<button onclick='clearConsole()'>Clear</button>"
"</div>"
"<script>"
"let lastLength=0;"
"function refresh(){"
"fetch('/logs').then(r=>r.text()).then(data=>{"
"if(data.length>lastLength){document.getElementById('console').textContent=data;lastLength=data.length;}"
"document.getElementById('console').scrollTop=document.getElementById('console').scrollHeight;"
"});"
"}"
"function clearConsole(){fetch('/clear');lastLength=0;document.getElementById('console').textContent='';}"
"setInterval(refresh,2000);"
"refresh();"
"</script>"
"</body>"
"</html>";

/**
 * @brief Custom log function that sends to both UART and buffer
 */
static int webserial_log_func(const char *fmt, va_list args)
{
    // Call original log function (UART output)
    int ret = 0;
    if (original_log_func) {
        va_list args_copy;
        va_copy(args_copy, args);
        ret = original_log_func(fmt, args_copy);
        va_end(args_copy);
    }

    // Format message for buffer
    char message[256];
    int len = vsnprintf(message, sizeof(message), fmt, args);

    if (len > 0 && len < sizeof(message)) {
        // Add to circular log buffer
        xSemaphoreTake(log_mutex, portMAX_DELAY);

        if (log_buffer_pos + len >= LOG_BUFFER_SIZE) {
            // Buffer full, shift contents
            memmove(log_buffer, log_buffer + LOG_BUFFER_SIZE / 2, LOG_BUFFER_SIZE / 2);
            log_buffer_pos = LOG_BUFFER_SIZE / 2;
        }

        memcpy(log_buffer + log_buffer_pos, message, len);
        log_buffer_pos += len;
        log_buffer[log_buffer_pos] = '\0';

        xSemaphoreGive(log_mutex);
    }

    return ret;
}

/**
 * @brief Logs endpoint - returns current log buffer
 */
static esp_err_t logs_handler(httpd_req_t *req)
{
    client_count++;

    xSemaphoreTake(log_mutex, portMAX_DELAY);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, log_buffer, log_buffer_pos);

    xSemaphoreGive(log_mutex);

    return ESP_OK;
}

/**
 * @brief Clear endpoint - clears log buffer
 */
static esp_err_t clear_handler(httpd_req_t *req)
{
    xSemaphoreTake(log_mutex, portMAX_DELAY);

    log_buffer_pos = 0;
    log_buffer[0] = '\0';

    xSemaphoreGive(log_mutex);

    httpd_resp_send(req, "OK", 2);

    ESP_LOGI(TAG, "Log buffer cleared");

    return ESP_OK;
}

/**
 * @brief Root page handler - serves HTML interface
 */
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, webserial_html, strlen(webserial_html));
}

/**
 * @brief Broadcast message to log buffer (for compatibility)
 */
esp_err_t webserial_broadcast(const char *message, size_t length)
{
    // Message already captured by log hook
    return ESP_OK;
}

/**
 * @brief Initialize WebSerial server
 */
esp_err_t webserial_init(void)
{
    if (server_running) {
        ESP_LOGW(TAG, "WebSerial already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WebSerial server...");

    // Create mutex
    log_mutex = xSemaphoreCreateMutex();
    if (!log_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;

    // Start HTTP server
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t logs_uri = {
        .uri       = "/logs",
        .method    = HTTP_GET,
        .handler   = logs_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &logs_uri);

    httpd_uri_t clear_uri = {
        .uri       = "/clear",
        .method    = HTTP_GET,
        .handler   = clear_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &clear_uri);

    // Hook into ESP log system
    original_log_func = esp_log_set_vprintf(webserial_log_func);

    server_running = true;
    ESP_LOGI(TAG, "WebSerial server started successfully");
    ESP_LOGI(TAG, "Access WebSerial at: http://<device-ip>/");

    return ESP_OK;
}

/**
 * @brief Deinitialize WebSerial server
 */
esp_err_t webserial_deinit(void)
{
    if (!server_running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping WebSerial server...");

    // Restore original log function
    if (original_log_func) {
        esp_log_set_vprintf(original_log_func);
        original_log_func = NULL;
    }

    // Stop HTTP server
    if (server) {
        httpd_stop(server);
        server = NULL;
    }

    // Delete mutex
    if (log_mutex) {
        vSemaphoreDelete(log_mutex);
        log_mutex = NULL;
    }

    server_running = false;
    client_count = 0;

    ESP_LOGI(TAG, "WebSerial server stopped");
    return ESP_OK;
}

/**
 * @brief Check if WebSerial is running
 */
bool webserial_is_running(void)
{
    return server_running;
}

/**
 * @brief Get number of connected WebSerial clients
 */
int webserial_get_client_count(void)
{
    return client_count;
}
