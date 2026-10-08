#include "ha_lights.h"
#include "network_service.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ha_lights";
static const char *const s_ids[HA_LIGHT_COUNT] = {
    "light.yeelink_ceil40_9771_light",       /* 客厅灯 */
    "light.yeelink_ceil40_d8b6_light",       /* 书房灯 */
    "light.yeelink_ceiling17_b415_light",    /* 卧室灯 */
    "light.yeelink_bslamp2_304c_light",      /* 床头灯 */
    "light.yeelink_ceiling17_40d7_light",    /* 小卧室灯 */
    "light.yeelink_stripa_ba24_light",       /* 彩光灯带 */
    "light.yeelink_v20_6acb_light",          /* 浴室灯 -> 浴霸灯 */
    "light.xiaomi_0002_bfe9_light"           /* 阳台灯 -> 晾衣架灯 */
};
typedef struct {
    int index;
    ha_light_command_t type;
    int value;
    int extra;
} command_t;
static QueueHandle_t s_commands;
static SemaphoreHandle_t s_lock;
static ha_light_state_t s_states[HA_LIGHT_COUNT];
static network_backend_config_t s_config;

const char *ha_lights_entity_id(int index)
{
    return index >= 0 && index < HA_LIGHT_COUNT ? s_ids[index] : NULL;
}

bool ha_lights_get(int index, ha_light_state_t *out)
{
    if (!out || index < 0 || index >= HA_LIGHT_COUNT || !s_lock) return false;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) != pdTRUE) return false;
    *out = s_states[index];
    xSemaphoreGive(s_lock);
    return true;
}

bool ha_lights_send(int index, ha_light_command_t type, int value, int extra)
{
    if (index < 0 || index >= HA_LIGHT_COUNT || !s_commands) return false;
    command_t cmd = { index, type, value, extra };
    return xQueueSend(s_commands, &cmd, 0) == pdTRUE;
}

typedef struct { char data[2048]; size_t len; } response_t;
static esp_err_t http_event(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA || !evt->user_data) return ESP_OK;
    response_t *r = evt->user_data;
    if (evt->data_len <= 0 || r->len + (size_t)evt->data_len >= sizeof(r->data))
        return ESP_FAIL;
    memcpy(r->data + r->len, evt->data, evt->data_len);
    r->len += evt->data_len;
    r->data[r->len] = 0;
    return ESP_OK;
}

static bool request(const char *path, const char *body, response_t *reply)
{
    char url[NETWORK_HA_URL_MAX + 140];
    size_t len = strlen(s_config.ha_url);
    while (len && s_config.ha_url[len - 1] == '/') len--;
    if (len == 0 || len + strlen(path) >= sizeof(url)) return false;
    snprintf(url, sizeof(url), "%.*s%s", (int)len, s_config.ha_url, path);
    esp_http_client_config_t cfg = {
        .url = url, .timeout_ms = 3500, .event_handler = http_event,
        .user_data = reply, .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    char auth[NETWORK_HA_TOKEN_MAX + 8];
    snprintf(auth, sizeof(auth), "Bearer %s", s_config.ha_token);
    esp_http_client_set_header(client, "Authorization", auth);
    if (body) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body, strlen(body));
    }
    if (reply) memset(reply, 0, sizeof(*reply));
    esp_err_t err = esp_http_client_perform(client);
    int status = err == ESP_OK ? esp_http_client_get_status_code(client) : 0;
    esp_http_client_cleanup(client);
    if (err != ESP_OK || status < 200 || status >= 300) {
        ESP_LOGW(TAG, "HA %s failed: http=%d err=%s", path, status, esp_err_to_name(err));
        return false;
    }
    return true;
}

static void refresh_one(int i)
{
    char path[120];
    snprintf(path, sizeof(path), "/api/states/%s", s_ids[i]);
    response_t *reply = calloc(1, sizeof(*reply));
    if (!reply) return;
    bool ok = request(path, NULL, reply);
    ha_light_state_t state = {0};
    if (ok) {
        cJSON *root = cJSON_Parse(reply->data);
        const cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "state");
        const cJSON *attrs = cJSON_GetObjectItemCaseSensitive(root, "attributes");
        if (cJSON_IsString(status) && status->valuestring &&
            (!strcmp(status->valuestring, "on") || !strcmp(status->valuestring, "off"))) {
            state.available = true;
            state.on = !strcmp(status->valuestring, "on");
            const cJSON *v = cJSON_GetObjectItemCaseSensitive(attrs, "brightness");
            state.brightness_pct = cJSON_IsNumber(v) ? (uint8_t)((v->valuedouble * 100.0 / 255.0) + 0.5) : 0;
            v = cJSON_GetObjectItemCaseSensitive(attrs, "color_temp_kelvin");
            state.color_temp_k = cJSON_IsNumber(v) ? (uint16_t)v->valueint : 0;
            const cJSON *hs = cJSON_GetObjectItemCaseSensitive(attrs, "hs_color");
            if (cJSON_IsArray(hs) && cJSON_GetArraySize(hs) >= 2) {
                v = cJSON_GetArrayItem(hs, 0);
                if (cJSON_IsNumber(v)) state.hue_deg = (uint16_t)v->valuedouble;
                v = cJSON_GetArrayItem(hs, 1);
                if (cJSON_IsNumber(v)) state.saturation_pct = (uint8_t)(v->valuedouble + 0.5);
            }
        }
        cJSON_Delete(root);
    }
    free(reply);
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_states[i] = state;
        xSemaphoreGive(s_lock);
    }
}

static void send_one(const command_t *cmd)
{
    const char *service = (cmd->type == HA_LIGHT_POWER && !cmd->value) ? "turn_off" : "turn_on";
    char path[64], body[256];
    snprintf(path, sizeof(path), "/api/services/light/%s", service);
    switch (cmd->type) {
    case HA_LIGHT_POWER:
        snprintf(body, sizeof(body), "{\"entity_id\":\"%s\"}", s_ids[cmd->index]); break;
    case HA_LIGHT_BRIGHTNESS:
        snprintf(body, sizeof(body), "{\"entity_id\":\"%s\",\"brightness_pct\":%d}",
                 s_ids[cmd->index], cmd->value); break;
    case HA_LIGHT_TEMPERATURE:
        snprintf(body, sizeof(body), "{\"entity_id\":\"%s\",\"color_temp_kelvin\":%d}",
                 s_ids[cmd->index], cmd->value); break;
    case HA_LIGHT_COLOR:
        snprintf(body, sizeof(body), "{\"entity_id\":\"%s\",\"hs_color\":[%d,%d]}",
                 s_ids[cmd->index], cmd->value, cmd->extra); break;
    default: return;
    }
    /* POST /api/services may return a large list of changed states; no response capture needed. */
    if (request(path, body, NULL)) refresh_one(cmd->index);
}

static void worker(void *arg)
{
    (void)arg;
    TickType_t last_poll = 0;
    for (;;) {
        network_wifi_status_t wifi;
        network_service_get_wifi_status(&wifi);
        if (!wifi.connected) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        command_t cmd;
        if (xQueueReceive(s_commands, &cmd, pdMS_TO_TICKS(200)) == pdTRUE) {
            send_one(&cmd);
            continue;
        }
        if (last_poll == 0 || (xTaskGetTickCount() - last_poll) >= pdMS_TO_TICKS(10000)) {
            last_poll = xTaskGetTickCount();
            for (int i = 0; i < HA_LIGHT_COUNT; i++) {
                if (xQueueReceive(s_commands, &cmd, 0) == pdTRUE) send_one(&cmd);
                refresh_one(i);
            }
        }
    }
}

esp_err_t ha_lights_init(void)
{
    if (s_commands) return ESP_OK;
    network_service_get_backend_config(&s_config);
    if (!s_config.ha_url[0] || !s_config.ha_token[0]) {
        ESP_LOGI(TAG, "HA not configured in TF card");
        return ESP_ERR_NOT_FOUND;
    }
    s_lock = xSemaphoreCreateMutex();
    s_commands = xQueueCreate(20, sizeof(command_t));
    if (!s_lock || !s_commands) return ESP_ERR_NO_MEM;
    if (xTaskCreate(worker, "ha_lights", 6144, NULL, 3, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "HA light bridge started for %d entities", HA_LIGHT_COUNT);
    return ESP_OK;
}
