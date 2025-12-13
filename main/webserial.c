/**
 * @file webserial.c
 * @brief WebSerial implementation - Remote serial console
 */

#include "webserial.h"
#include "va_control.h"
#include "bsp/esp32_p4_function_ev_board.h"
#include "ha_client.h"
#include "local_music_player.h"
#include "mqtt_ha.h"
#include "network_manager.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <strings.h>
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

static const char *dashboard_html =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32-P4 Voice Assistant</title>"
"<style>"
"body{font-family:system-ui,Segoe UI,Arial,sans-serif;margin:0;padding:16px;background:#0f1115;color:#e6e6e6}"
"a{color:#7db7ff}"
".wrap{max-width:960px;margin:0 auto}"
".row{display:flex;flex-wrap:wrap;gap:12px}"
".card{background:#171a21;border:1px solid #2a2f3a;border-radius:12px;padding:14px;flex:1;min-width:280px}"
".k{color:#9aa4b2}"
".v{font-family:ui-monospace,Consolas,monospace}"
"label{display:block;margin:10px 0 6px;color:#cdd6e3}"
"input,select{width:100%;padding:10px;border-radius:10px;border:1px solid #2a2f3a;background:#0f1115;color:#e6e6e6}"
"button{padding:10px 14px;border-radius:10px;border:1px solid #2a2f3a;background:#2b6cb0;color:#fff;cursor:pointer}"
"button.secondary{background:#2a2f3a}"
"button.danger{background:#b02b2b}"
".grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px}"
".small{font-size:12px;color:#9aa4b2}"
"</style></head><body><div class='wrap'>"
"<h2>ESP32-P4 Voice Assistant</h2>"
"<div class='small'>WebSerial: <a href='/webserial'>/webserial</a></div>"
"<div class='row'>"
"<div class='card' style='flex:2'>"
"<h3>Status</h3>"
"<div class='grid2'>"
"<div><div class='k'>IP</div><div class='v' id='ip'>-</div></div>"
"<div><div class='k'>Uptime</div><div class='v' id='uptime'>-</div></div>"
"<div><div class='k'>Free heap</div><div class='v' id='heap'>-</div></div>"
"<div><div class='k'>SD</div><div class='v' id='sd'>-</div></div>"
"<div><div class='k'>HA</div><div class='v' id='ha'>-</div></div>"
"<div><div class='k'>MQTT</div><div class='v' id='mqtt'>-</div></div>"
"<div><div class='k'>WWD</div><div class='v' id='wwd'>-</div></div>"
"<div><div class='k'>AGC</div><div class='v' id='agc'>-</div></div>"
"</div>"
"<div style='margin-top:12px' class='row'>"
"<button class='secondary' onclick=\"action('wwd_resume')\">WWD ON</button>"
"<button class='secondary' onclick=\"action('wwd_stop')\">WWD OFF</button>"
"<button class='danger' onclick=\"action('restart')\">Restart</button>"
"</div>"
"</div>"
"<div class='card'>"
"<h3>Config</h3>"
"<label>WWD threshold (0-1)</label><input id='wwd_threshold' type='number' min='0.05' max='0.99' step='0.01'>"
"<label>VAD threshold</label><input id='vad_threshold' type='number' min='50' max='300' step='1'>"
"<label>VAD silence (ms)</label><input id='vad_silence' type='number' min='200' max='10000' step='10'>"
"<label>VAD min speech (ms)</label><input id='vad_min' type='number' min='50' max='5000' step='10'>"
"<label>VAD max recording (ms)</label><input id='vad_max' type='number' min='500' max='20000' step='50'>"
"<label>AGC</label>"
"<select id='agc_enabled'><option value='1'>Enabled</option><option value='0'>Disabled</option></select>"
"<label>AGC target level</label><input id='agc_target' type='number' min='500' max='12000' step='10'>"
"<div style='margin-top:12px' class='row'>"
"<button onclick='save()'>Save</button>"
"<button class='secondary' onclick='refresh()'>Refresh</button>"
"</div>"
"</div>"
"</div>"
"<script>"
"function fmtBool(b){return b?'yes':'no'}"
"function secondsToHMS(s){s=Math.max(0,Math.floor(s));const h=Math.floor(s/3600);const m=Math.floor((s%3600)/60);const ss=s%60;return `${h}h ${m}m ${ss}s`}"
"function refresh(){fetch('/api/status').then(r=>r.json()).then(j=>{"
"document.getElementById('ip').textContent=j.ip||'-';"
"document.getElementById('uptime').textContent=secondsToHMS(j.uptime_s||0);"
"document.getElementById('heap').textContent=(j.free_heap||0)+' bytes';"
"document.getElementById('sd').textContent=j.sd_mounted?'mounted':'unmounted';"
"document.getElementById('ha').textContent=fmtBool(j.ha_connected);"
"document.getElementById('mqtt').textContent=fmtBool(j.mqtt_connected);"
"document.getElementById('wwd').textContent=j.wwd_running?'running':'stopped';"
"document.getElementById('agc').textContent=(j.agc_enabled?'enabled':'disabled')+' (target '+(j.agc_target||0)+')';"
"document.getElementById('wwd_threshold').value=j.wwd_threshold;"
"document.getElementById('vad_threshold').value=j.vad_threshold;"
"document.getElementById('vad_silence').value=j.vad_silence_ms;"
"document.getElementById('vad_min').value=j.vad_min_speech_ms;"
"document.getElementById('vad_max').value=j.vad_max_recording_ms;"
"document.getElementById('agc_enabled').value=j.agc_enabled? '1':'0';"
"document.getElementById('agc_target').value=j.agc_target;"
"});}"
"function post(path, obj){return fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(obj)});}"
"function save(){post('/api/config', {"
"wwd_threshold:document.getElementById('wwd_threshold').value,"
"vad_threshold:document.getElementById('vad_threshold').value,"
"vad_silence_ms:document.getElementById('vad_silence').value,"
"vad_min_speech_ms:document.getElementById('vad_min').value,"
"vad_max_recording_ms:document.getElementById('vad_max').value,"
"agc_enabled:document.getElementById('agc_enabled').value,"
"agc_target:document.getElementById('agc_target').value"
"}).then(()=>refresh());}"
"function action(cmd){post('/api/action',{cmd}).then(()=>refresh());}"
"setInterval(refresh,2000);refresh();"
"</script></div></body></html>";

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
 "<a href='/' style='color:#7db7ff'>← Dashboard</a> "
 "<button onclick='refresh()'>Refresh Now</button>"
 "<button onclick='clearConsole()'>Clear</button>"
 "</div>"
 "<script>"
 "let lastLength=0;"
 "function refresh(){"
 "fetch('/webserial/logs').then(r=>r.text()).then(data=>{"
 "if(data.length>lastLength){document.getElementById('console').textContent=data;lastLength=data.length;}"
 "document.getElementById('console').scrollTop=document.getElementById('console').scrollHeight;"
 "});"
 "}"
 "function clearConsole(){fetch('/webserial/clear');lastLength=0;document.getElementById('console').textContent='';}"
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

static void url_decode_inplace(char *s) {
    if (s == NULL) {
        return;
    }

    char *src = s;
    char *dst = s;

    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char)strtoul(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static bool form_get_param(const char *body, const char *key, char *out, size_t out_len) {
    if (!body || !key || !out || out_len == 0) {
        return false;
    }

    size_t key_len = strlen(key);
    const char *p = body;

    while (p && *p) {
        if ((strncmp(p, key, key_len) == 0) && p[key_len] == '=') {
            const char *val = p + key_len + 1;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= out_len) {
                len = out_len - 1;
            }
            memcpy(out, val, len);
            out[len] = '\0';
            url_decode_inplace(out);
            return true;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return false;
}

static esp_err_t recv_body(httpd_req_t *req, char *buf, size_t buf_len) {
    if (buf == NULL || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t total = req->content_len;
    if (total >= buf_len) {
        total = buf_len - 1;
    }

    size_t received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0) {
            return ESP_FAIL;
        }
        received += (size_t)r;
    }
    buf[received] = '\0';
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
 * @brief Dashboard page handler - serves HTML interface
 */
static esp_err_t dashboard_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, dashboard_html, strlen(dashboard_html));
}

static esp_err_t webserial_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, webserial_html, strlen(webserial_html));
}

static esp_err_t api_status_handler(httpd_req_t *req)
{
    char ip_str[16] = "";
    if (network_manager_get_ip(ip_str) != ESP_OK) {
        strncpy(ip_str, "0.0.0.0", sizeof(ip_str) - 1);
    }

    uint64_t uptime_s = (uint64_t)(esp_timer_get_time() / 1000000ULL);

    bool sd_mounted = (bsp_sdcard != NULL);
    bool ha_connected = ha_client_is_connected();
    bool mqtt_connected = mqtt_ha_is_connected();

    char json[512];
    int n = snprintf(json, sizeof(json),
                     "{"
                     "\"ip\":\"%s\","
                     "\"uptime_s\":%llu,"
                     "\"free_heap\":%lu,"
                     "\"sd_mounted\":%s,"
                     "\"ha_connected\":%s,"
                     "\"mqtt_connected\":%s,"
                     "\"wwd_running\":%s,"
                     "\"pipeline_active\":%s,"
                     "\"wwd_threshold\":%.2f,"
                     "\"vad_threshold\":%lu,"
                     "\"vad_silence_ms\":%lu,"
                     "\"vad_min_speech_ms\":%lu,"
                     "\"vad_max_recording_ms\":%lu,"
                     "\"agc_enabled\":%s,"
                     "\"agc_target\":%u"
                     "}",
                     ip_str,
                     (unsigned long long)uptime_s,
                     (unsigned long)esp_get_free_heap_size(),
                     sd_mounted ? "true" : "false",
                     ha_connected ? "true" : "false",
                     mqtt_connected ? "true" : "false",
                     va_control_get_wwd_running() ? "true" : "false",
                     va_control_get_pipeline_active() ? "true" : "false",
                     (double)va_control_get_wwd_threshold(),
                     (unsigned long)va_control_get_vad_threshold(),
                     (unsigned long)va_control_get_vad_silence_duration_ms(),
                     (unsigned long)va_control_get_vad_min_speech_ms(),
                     (unsigned long)va_control_get_vad_max_recording_ms(),
                     va_control_get_agc_enabled() ? "true" : "false",
                     (unsigned int)va_control_get_agc_target_level());
    if (n < 0) {
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t api_action_handler(httpd_req_t *req)
{
    char body[256];
    if (recv_body(req, body, sizeof(body)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
        return ESP_OK;
    }

    char cmd[32] = {0};
    if (!form_get_param(body, "cmd", cmd, sizeof(cmd))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing cmd");
        return ESP_OK;
    }

    if (strcmp(cmd, "restart") == 0) {
        va_control_action_restart();
    } else if (strcmp(cmd, "wwd_resume") == 0) {
        va_control_action_wwd_resume();
    } else if (strcmp(cmd, "wwd_stop") == 0) {
        va_control_action_wwd_stop();
    } else if (strcmp(cmd, "test_tts") == 0) {
        char text[128] = {0};
        if (form_get_param(body, "text", text, sizeof(text)) && text[0] != '\0') {
            va_control_action_test_tts(text);
        }
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t api_config_handler(httpd_req_t *req)
{
    char body[512];
    if (recv_body(req, body, sizeof(body)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
        return ESP_OK;
    }

    char tmp[32];

    if (form_get_param(body, "wwd_threshold", tmp, sizeof(tmp))) {
        va_control_set_wwd_threshold(strtof(tmp, NULL));
    }
    if (form_get_param(body, "vad_threshold", tmp, sizeof(tmp))) {
        va_control_set_vad_threshold((uint32_t)strtoul(tmp, NULL, 10));
    }
    if (form_get_param(body, "vad_silence_ms", tmp, sizeof(tmp))) {
        va_control_set_vad_silence_duration_ms((uint32_t)strtoul(tmp, NULL, 10));
    }
    if (form_get_param(body, "vad_min_speech_ms", tmp, sizeof(tmp))) {
        va_control_set_vad_min_speech_ms((uint32_t)strtoul(tmp, NULL, 10));
    }
    if (form_get_param(body, "vad_max_recording_ms", tmp, sizeof(tmp))) {
        va_control_set_vad_max_recording_ms((uint32_t)strtoul(tmp, NULL, 10));
    }

    if (form_get_param(body, "agc_enabled", tmp, sizeof(tmp))) {
        va_control_set_agc_enabled((tmp[0] == '1') || (strcasecmp(tmp, "true") == 0));
    }
    if (form_get_param(body, "agc_target", tmp, sizeof(tmp))) {
        va_control_set_agc_target_level((uint16_t)strtoul(tmp, NULL, 10));
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
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
        .handler   = dashboard_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t status_uri = {
        .uri       = "/api/status",
        .method    = HTTP_GET,
        .handler   = api_status_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &status_uri);

    httpd_uri_t action_uri = {
        .uri       = "/api/action",
        .method    = HTTP_POST,
        .handler   = api_action_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &action_uri);

    httpd_uri_t config_uri = {
        .uri       = "/api/config",
        .method    = HTTP_POST,
        .handler   = api_config_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &config_uri);

    httpd_uri_t webserial_uri = {
        .uri       = "/webserial",
        .method    = HTTP_GET,
        .handler   = webserial_page_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &webserial_uri);

    httpd_uri_t logs_uri = {
        .uri       = "/webserial/logs",
        .method    = HTTP_GET,
        .handler   = logs_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &logs_uri);

    httpd_uri_t clear_uri = {
        .uri       = "/webserial/clear",
        .method    = HTTP_GET,
        .handler   = clear_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &clear_uri);

    // Hook into ESP log system
    original_log_func = esp_log_set_vprintf(webserial_log_func);

    server_running = true;
    ESP_LOGI(TAG, "WebSerial server started successfully");
    ESP_LOGI(TAG, "Dashboard: http://<device-ip>/");
    ESP_LOGI(TAG, "WebSerial:  http://<device-ip>/webserial");

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
