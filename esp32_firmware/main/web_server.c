#include "web_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "cJSON.h"
#include "config_storage.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "web"

static const char *INDEX_HTML =
    "<!DOCTYPE html>\n"
    "<html lang=\"ru\">\n"
    "<head><meta charset=\"UTF-8\"><title>BLDC Control</title>"
    "<style>body{font-family:sans-serif;margin:20px;}button{margin:4px;}label{display:block;margin-top:8px;}</style>"
    "</head><body>"
    "<h1>BLDC контроллер</h1>"
    "<div id=\"status\"></div>"
    "<button onclick=\"sendCommand('start_forward')\">Старт вперёд</button>"
    "<button onclick=\"sendCommand('start_reverse')\">Старт назад</button>"
    "<button onclick=\"sendCommand('reverse')\">Принудительный реверс</button>"
    "<button onclick=\"sendCommand('stop')\">Стоп</button>"
    "<h2>Параметры</h2>"
    "<form onsubmit=\"saveConfig(event)\">"
    "<label>Макс RPM <input type=\"number\" id=\"target_rpm\"></label>"
    "<label>Разгон (мс) <input type=\"number\" id=\"ramp_up_ms\"></label>"
    "<label>Торможение (мс) <input type=\"number\" id=\"ramp_down_ms\"></label>"
    "<label>Ток нагрузки (А) <input type=\"number\" step=\"0.1\" id=\"high_load_current_a\"></label>"
    "<label>Порог отрыва (А) <input type=\"number\" step=\"0.1\" id=\"low_load_current_a\"></label>"
    "<label>Задержка после отрыва (мс) <input type=\"number\" id=\"load_drop_delay_ms\"></label>"
    "<label>Задержка стабилизации (мс) <input type=\"number\" id=\"settle_delay_ms\"></label>"
    "<button type=\"submit\">Сохранить</button>"
    "</form>"
    "<h2>Логирование</h2>"
    "<label><input type=\"checkbox\" id=\"logging_enabled\" onchange=\"toggleLogging(this)\"> Запись CSV</label>"
    "<a href=\"/api/log\" target=\"_blank\">Скачать лог</a>"
    "<script>"
    "async function refresh(){const r=await fetch('/api/status');const d=await r.json();"
    "document.getElementById('status').innerText=`Состояние: ${d.state}, скорость ${d.telemetry.speed_rpm.toFixed(0)} RPM, ток ${d.telemetry.current_a.toFixed(1)} А`;"
    "for(const k of Object.keys(d.config)){const el=document.getElementById(k);if(el){el.value=d.config[k];}}"
    "document.getElementById('logging_enabled').checked=d.config.logging_enabled;"
    "}"
    "async function sendCommand(action){await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action})});refresh();}"
    "async function saveConfig(ev){ev.preventDefault();const payload={};['target_rpm','ramp_up_ms','ramp_down_ms','high_load_current_a','low_load_current_a','load_drop_delay_ms','settle_delay_ms','logging_enabled'].forEach(k=>{const el=document.getElementById(k);if(el){payload[k]=parseFloat(el.value);}});payload.logging_enabled=document.getElementById('logging_enabled').checked;await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});refresh();}"
    "async function toggleLogging(el){await fetch('/api/logging',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:el.checked})});refresh();}"
    "setInterval(refresh,1000);refresh();"
    "</script>"
    "</body></html>";

static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t command_post_handler(httpd_req_t *req);
static esp_err_t config_post_handler(httpd_req_t *req);
static esp_err_t logging_post_handler(httpd_req_t *req);
static esp_err_t log_download_handler(httpd_req_t *req);
static const char *state_to_string(app_state_t state);
static cJSON *telemetry_to_json(const esc_telemetry_t *telemetry);

static bool s_wifi_initialized = false;

esp_err_t web_server_init(web_server_t *srv,
                          state_machine_t *sm,
                          app_config_t *config,
                          telemetry_log_t *log,
                          uart_comm_t *uart)
{
    memset(srv, 0, sizeof(*srv));
    srv->server = NULL;
    srv->running = false;
    srv->sm = sm;
    srv->config = config;
    srv->log = log;
    srv->uart = uart;
    return ESP_OK;
}

esp_err_t web_server_start_ap(web_server_t *srv)
{
    if (srv->running)
    {
        return ESP_OK;
    }

    if (!s_wifi_initialized)
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        s_wifi_initialized = true;
    }

    wifi_config_t ap_config = {0};
    strlcpy((char *)ap_config.ap.ssid, APP_WIFI_AP_SSID, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(APP_WIFI_AP_SSID);
    strlcpy((char *)ap_config.ap.password, APP_WIFI_AP_PASSWORD, sizeof(ap_config.ap.password));
    ap_config.ap.channel = APP_WIFI_CHANNEL;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = (strlen(APP_WIFI_AP_PASSWORD) < 8) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.ap.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&srv->server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Не удалось запустить HTTP сервер: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = srv};
    httpd_uri_t status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler, .user_ctx = srv};
    httpd_uri_t command = {.uri = "/api/command", .method = HTTP_POST, .handler = command_post_handler, .user_ctx = srv};
    httpd_uri_t cfg_uri = {.uri = "/api/config", .method = HTTP_POST, .handler = config_post_handler, .user_ctx = srv};
    httpd_uri_t logging = {.uri = "/api/logging", .method = HTTP_POST, .handler = logging_post_handler, .user_ctx = srv};
    httpd_uri_t log_uri = {.uri = "/api/log", .method = HTTP_GET, .handler = log_download_handler, .user_ctx = srv};

    httpd_register_uri_handler(srv->server, &root);
    httpd_register_uri_handler(srv->server, &status);
    httpd_register_uri_handler(srv->server, &command);
    httpd_register_uri_handler(srv->server, &cfg_uri);
    httpd_register_uri_handler(srv->server, &logging);
    httpd_register_uri_handler(srv->server, &log_uri);

    srv->running = true;
    ESP_LOGI(TAG, "Точка доступа запущена: %s", APP_WIFI_AP_SSID);
    return ESP_OK;
}

void web_server_stop(web_server_t *srv)
{
    if (!srv->running)
    {
        return;
    }
    if (srv->server != NULL)
    {
        httpd_stop(srv->server);
        srv->server = NULL;
    }
    esp_wifi_stop();
    srv->running = false;
    ESP_LOGI(TAG, "Веб-сервер остановлен");
}

bool web_server_is_running(const web_server_t *srv)
{
    return srv->running;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    web_server_t *srv = (web_server_t *)req->user_ctx;
    esc_telemetry_t telemetry = {0};
    uart_comm_get_latest(srv->uart, &telemetry);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", state_to_string(state_machine_get_state(srv->sm)));
    cJSON_AddBoolToObject(root, "wifi", state_machine_is_wifi_mode(srv->sm));

    cJSON *cfg = cJSON_CreateObject();
    cJSON_AddNumberToObject(cfg, "target_rpm", srv->config->target_rpm);
    cJSON_AddNumberToObject(cfg, "ramp_up_ms", srv->config->ramp_up_ms);
    cJSON_AddNumberToObject(cfg, "ramp_down_ms", srv->config->ramp_down_ms);
    cJSON_AddNumberToObject(cfg, "high_load_current_a", srv->config->high_load_current_a);
    cJSON_AddNumberToObject(cfg, "low_load_current_a", srv->config->low_load_current_a);
    cJSON_AddNumberToObject(cfg, "load_drop_delay_ms", srv->config->load_drop_delay_ms);
    cJSON_AddNumberToObject(cfg, "settle_delay_ms", srv->config->settle_delay_ms);
    cJSON_AddBoolToObject(cfg, "logging_enabled", srv->config->logging_enabled);
    cJSON_AddItemToObject(root, "config", cfg);

    cJSON *telemetry_json = telemetry_to_json(&telemetry);
    cJSON_AddItemToObject(root, "telemetry", telemetry_json);

    char *payload = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t res = httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
    cJSON_Delete(root);
    free(payload);
    return res;
}

static char *read_body(httpd_req_t *req)
{
    int total = req->content_len;
    char *buffer = calloc(total + 1, 1);
    if (!buffer)
    {
        return NULL;
    }
    int received = 0;
    while (received < total)
    {
        int ret = httpd_req_recv(req, buffer + received, total - received);
        if (ret <= 0)
        {
            free(buffer);
            return NULL;
        }
        received += ret;
    }
    buffer[total] = '\0';
    return buffer;
}

static esp_err_t command_post_handler(httpd_req_t *req)
{
    web_server_t *srv = (web_server_t *)req->user_ctx;
    char *body = read_body(req);
    if (!body)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
    }
    cJSON *json = cJSON_Parse(body);
    free(body);
    if (!json)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");
    }
    const cJSON *action = cJSON_GetObjectItem(json, "action");
    if (!cJSON_IsString(action))
    {
        cJSON_Delete(json);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing action");
    }
    if (strcmp(action->valuestring, "start_forward") == 0)
    {
        state_machine_command_start(srv->sm, true);
    }
    else if (strcmp(action->valuestring, "start_reverse") == 0)
    {
        state_machine_command_start(srv->sm, false);
    }
    else if (strcmp(action->valuestring, "stop") == 0)
    {
        state_machine_command_stop(srv->sm);
    }
    else if (strcmp(action->valuestring, "reverse") == 0)
    {
        state_machine_command_reverse(srv->sm);
    }
    else if (strcmp(action->valuestring, "calibrate") == 0)
    {
        uart_comm_send_calibrate(srv->uart);
    }
    cJSON_Delete(json);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"result\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t config_post_handler(httpd_req_t *req)
{
    web_server_t *srv = (web_server_t *)req->user_ctx;
    char *body = read_body(req);
    if (!body)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
    }
    cJSON *json = cJSON_Parse(body);
    free(body);
    if (!json)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");
    }

    cJSON *val;
    if ((val = cJSON_GetObjectItem(json, "target_rpm")) && cJSON_IsNumber(val))
    {
        srv->config->target_rpm = (uint32_t)val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(json, "ramp_up_ms")) && cJSON_IsNumber(val))
    {
        srv->config->ramp_up_ms = (uint32_t)val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(json, "ramp_down_ms")) && cJSON_IsNumber(val))
    {
        srv->config->ramp_down_ms = (uint32_t)val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(json, "high_load_current_a")) && cJSON_IsNumber(val))
    {
        srv->config->high_load_current_a = val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(json, "low_load_current_a")) && cJSON_IsNumber(val))
    {
        srv->config->low_load_current_a = val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(json, "load_drop_delay_ms")) && cJSON_IsNumber(val))
    {
        srv->config->load_drop_delay_ms = (uint32_t)val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(json, "settle_delay_ms")) && cJSON_IsNumber(val))
    {
        srv->config->settle_delay_ms = (uint32_t)val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(json, "logging_enabled")) && cJSON_IsBool(val))
    {
        srv->config->logging_enabled = cJSON_IsTrue(val);
    }

    config_storage_save(srv->config);
    cJSON_Delete(json);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"result\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t logging_post_handler(httpd_req_t *req)
{
    web_server_t *srv = (web_server_t *)req->user_ctx;
    char *body = read_body(req);
    if (!body)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
    }
    cJSON *json = cJSON_Parse(body);
    free(body);
    if (!json)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");
    }
    cJSON *enabled = cJSON_GetObjectItem(json, "enabled");
    if (cJSON_IsBool(enabled))
    {
        srv->config->logging_enabled = cJSON_IsTrue(enabled);
        config_storage_save(srv->config);
    }
    cJSON_Delete(json);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"result\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t log_download_handler(httpd_req_t *req)
{
    FILE *file = fopen(telemetry_log_get_path(), "r");
    if (!file)
    {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no file");
    }
    httpd_resp_set_type(req, "text/csv");
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file))
    {
        httpd_resp_send_chunk(req, buffer, HTTPD_RESP_USE_STRLEN);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const char *state_to_string(app_state_t state)
{
    switch (state)
    {
    case APP_STATE_IDLE: return "IDLE";
    case APP_STATE_STARTING_FORWARD: return "STARTING_FORWARD";
    case APP_STATE_RUNNING_FORWARD: return "RUNNING_FORWARD";
    case APP_STATE_STARTING_REVERSE: return "STARTING_REVERSE";
    case APP_STATE_RUNNING_REVERSE: return "RUNNING_REVERSE";
    case APP_STATE_BRAKING: return "BRAKING";
    case APP_STATE_FAULT: return "FAULT";
    default: return "UNKNOWN";
    }
}

static cJSON *telemetry_to_json(const esc_telemetry_t *telemetry)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "voltage_v", telemetry->voltage_v);
    cJSON_AddNumberToObject(obj, "current_a", telemetry->current_a);
    cJSON_AddNumberToObject(obj, "speed_rpm", telemetry->speed_rpm);
    cJSON_AddNumberToObject(obj, "temperature_c", telemetry->temperature_c);
    cJSON_AddNumberToObject(obj, "fault_mask", telemetry->fault_mask);
    cJSON_AddNumberToObject(obj, "motor_state", telemetry->motor_state);
    cJSON_AddNumberToObject(obj, "timestamp_ms", telemetry->timestamp_ms);
    cJSON_AddBoolToObject(obj, "home", telemetry->home_state);
    cJSON_AddBoolToObject(obj, "rew", telemetry->rew_state);
    return obj;
}
