#include "network_service.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "network";

static const char *NVS_NS = "ai_robot";
static const char *KEY_WIFI_SSID = "wifi_ssid";
static const char *KEY_WIFI_PASS = "wifi_pass";
static const char *KEY_WIFI_ENABLED = "wifi_on";
static const char *KEY_AI_URL = "ai_url";
static const char *KEY_HA_URL = "ha_url";
static const char *KEY_HA_TOKEN = "ha_token";

static const char *SETUP_SSID = "AI-Robot-Setup";
static const char *SETUP_URL = "http://192.168.4.1";

static bool s_initialized = false;
static bool s_wifi_enabled = true;
static bool s_wifi_started = false;
static bool s_sntp_started = false;
static bool s_setup_active = false;
static bool s_portal_stop_scheduled = false;

static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;
static httpd_handle_t s_httpd = NULL;
static TaskHandle_t s_dns_task = NULL;
static int s_dns_socket = -1;
static volatile bool s_dns_running = false;

static network_wifi_status_t s_status = {0};
static char s_wifi_password[NETWORK_WIFI_PASSWORD_MAX] = {0};
static network_backend_config_t s_backend = {0};

static esp_err_t nvs_read_string(nvs_handle_t nvs,
                                 const char *key,
                                 char *dst,
                                 size_t dst_size)
{
    size_t len = dst_size;
    esp_err_t err = nvs_get_str(nvs, key, dst, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        if (dst_size > 0) dst[0] = '\0';
        return ESP_OK;
    }
    return err;
}

static esp_err_t load_persistent_config(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;

    uint8_t wifi_on = 1;
    esp_err_t wifi_on_err = nvs_get_u8(nvs, KEY_WIFI_ENABLED, &wifi_on);
    if (wifi_on_err != ESP_OK && wifi_on_err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs);
        return wifi_on_err;
    }
    s_wifi_enabled = wifi_on != 0;

    err = nvs_read_string(nvs, KEY_WIFI_SSID,
                          s_status.ssid, sizeof(s_status.ssid));
    if (err == ESP_OK) {
        err = nvs_read_string(nvs, KEY_WIFI_PASS,
                              s_wifi_password, sizeof(s_wifi_password));
    }
    if (err == ESP_OK) {
        err = nvs_read_string(nvs, KEY_AI_URL,
                              s_backend.ai_url, sizeof(s_backend.ai_url));
    }
    if (err == ESP_OK) {
        err = nvs_read_string(nvs, KEY_HA_URL,
                              s_backend.ha_url, sizeof(s_backend.ha_url));
    }
    if (err == ESP_OK) {
        err = nvs_read_string(nvs, KEY_HA_TOKEN,
                              s_backend.ha_token, sizeof(s_backend.ha_token));
    }

    nvs_close(nvs);
    s_status.enabled = s_wifi_enabled;
    s_status.configured = s_status.ssid[0] != '\0';
    return err;
}

static esp_err_t save_string_pair(const char *key1, const char *value1,
                                  const char *key2, const char *value2)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs, key1, value1 ? value1 : "");
    if (err == ESP_OK) err = nvs_set_str(nvs, key2, value2 ? value2 : "");
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

static void time_sync_notification_cb(struct timeval *tv)
{
    if (tv == NULL) return;

    s_status.time_synced = true;

    time_t now = tv->tv_sec;
    struct tm local_tm;
    char stamp[32] = {0};
    if (localtime_r(&now, &local_tm) != NULL) {
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local_tm);
        ESP_LOGI(TAG, "SNTP synchronized: %s (TZ=CST-8 / UTC+8)", stamp);
    }
    else {
        ESP_LOGI(TAG, "SNTP synchronized");
    }
}

static void start_time_sync(void)
{
    if (s_sntp_started) return;

    /* Product deployment timezone. The clock page reads localtime(), so once
     * SNTP updates the system clock it changes automatically without a UI
     * special case. */
    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_notification_cb;
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SNTP init failed: %s", esp_err_to_name(err));
        return;
    }

    s_sntp_started = true;
    ESP_LOGI(TAG, "SNTP started");
}

static void refresh_link_metadata(void)
{
    if (s_sta_netif == NULL) return;

    uint8_t mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        snprintf(s_status.mac, sizeof(s_status.mac),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    esp_netif_dns_info_t dns;
    if (esp_netif_get_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK &&
        dns.ip.type == ESP_IPADDR_TYPE_V4) {
        const esp_ip4_addr_t *addr = &dns.ip.u_addr.ip4;
        snprintf(s_status.dns, sizeof(s_status.dns), IPSTR, IP2STR(addr));
    }
}

static esp_err_t configure_setup_ap(void)
{
    wifi_config_t ap = {0};
    strlcpy((char *)ap.ap.ssid, SETUP_SSID, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(SETUP_SSID);
    ap.ap.channel = 1;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 4;
    ap.ap.pmf_cfg.required = false;
    return esp_wifi_set_config(WIFI_IF_AP, &ap);
}

static esp_err_t ensure_wifi_runtime(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    if (!s_wifi_enabled && !s_setup_active) {
        if (s_wifi_started) {
            esp_err_t err = esp_wifi_stop();
            if (err != ESP_OK) return err;
            s_wifi_started = false;
        }
        return ESP_OK;
    }

    wifi_mode_t mode = WIFI_MODE_STA;
    if (s_setup_active) {
        mode = s_wifi_enabled ? WIFI_MODE_APSTA : WIFI_MODE_AP;
    }

    esp_err_t err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) return err;

    if (s_setup_active) {
        err = configure_setup_ap();
        if (err != ESP_OK) return err;
    }

    if (!s_wifi_started) {
        err = esp_wifi_start();
        if (err != ESP_OK) return err;
        s_wifi_started = true;
    }

    return ESP_OK;
}

static esp_err_t apply_wifi_config(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!s_wifi_enabled) return ESP_OK;

    esp_err_t err = ensure_wifi_runtime();
    if (err != ESP_OK) return err;

    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, s_status.ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, s_wifi_password, sizeof(cfg.sta.password));
    cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) return err;

    s_status.configured = s_status.ssid[0] != '\0';
    if (!s_status.configured) {
        (void)esp_wifi_disconnect();
        s_status.connected = false;
        return ESP_OK;
    }

    return esp_wifi_connect();
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(const char *src, size_t src_len,
                       char *dst, size_t dst_size)
{
    if (dst_size == 0) return;
    size_t out = 0;

    for (size_t i = 0; i < src_len && out + 1 < dst_size; ++i) {
        if (src[i] == '+') {
            dst[out++] = ' ';
        }
        else if (src[i] == '%' && i + 2 < src_len) {
            int hi = hex_value(src[i + 1]);
            int lo = hex_value(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                dst[out++] = (char)((hi << 4) | lo);
                i += 2;
            }
            else {
                dst[out++] = src[i];
            }
        }
        else {
            dst[out++] = src[i];
        }
    }
    dst[out] = '\0';
}

static bool form_get_value(const char *body,
                           const char *key,
                           char *dst,
                           size_t dst_size)
{
    if (body == NULL || key == NULL || dst == NULL || dst_size == 0) {
        return false;
    }

    const size_t key_len = strlen(key);
    const char *p = body;

    while (*p != '\0') {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : p + strlen(p);
        const char *eq = memchr(p, '=', (size_t)(end - p));

        if (eq != NULL &&
            (size_t)(eq - p) == key_len &&
            memcmp(p, key, key_len) == 0) {
            url_decode(eq + 1, (size_t)(end - eq - 1), dst, dst_size);
            return true;
        }

        if (amp == NULL) break;
        p = amp + 1;
    }

    return false;
}

static const char SETUP_PAGE[] =
"<!doctype html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>AI Robot Setup</title>"
"<style>"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
"background:#0a0c0f;color:#eef3f7;margin:0;padding:22px}"
".card{max-width:560px;margin:auto;background:#141920;border-radius:16px;padding:20px}"
"h2{margin:0 0 6px}p{color:#8e9aa7;margin:0 0 18px}"
"label{display:block;margin-top:13px;font-size:13px;color:#9aa6b2}"
"input{box-sizing:border-box;width:100%;margin-top:5px;padding:12px;border-radius:9px;"
"border:1px solid #34404d;background:#0d1116;color:#fff;font-size:16px}"
"button{width:100%;margin-top:20px;padding:13px;border:0;border-radius:10px;"
"background:#45d7f0;color:#071014;font-size:16px;font-weight:700}"
".section{margin-top:20px;padding-top:14px;border-top:1px solid #27313b}"
"</style></head><body><div class='card'>"
"<h2>AI Robot Setup</h2>"
"<p>Wi-Fi、AI 与 Home Assistant 配置</p>"
"<form method='post' action='/save'>"
"<label>Wi-Fi SSID</label><input name='ssid' autocomplete='off' required>"
"<label>Wi-Fi 密码</label><input name='password' type='password' "
"placeholder='留空则保留原密码'>"
"<div class='section'>"
"<label>AI Server URL</label><input name='ai_url' placeholder='http://192.168.50.149:8000'>"
"<label>Home Assistant URL</label><input name='ha_url' placeholder='http://192.168.50.x:8123'>"
"<label>HA Long-Lived Access Token</label><input name='ha_token' type='password' "
"placeholder='留空则保留现有 Token'>"
"</div><button type='submit'>保存并连接</button>"
"</form></div></body></html>";

static esp_err_t setup_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, SETUP_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t setup_save_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 2048) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid form");
        return ESP_FAIL;
    }

    char *body = calloc(1, req->content_len + 1);
    if (body == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    int received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, body + received,
                               req->content_len - received);
        if (r <= 0) {
            free(body);
            return ESP_FAIL;
        }
        received += r;
    }
    body[received] = '\0';

    char ssid[NETWORK_WIFI_SSID_MAX] = {0};
    char password[NETWORK_WIFI_PASSWORD_MAX] = {0};
    char ai_url[NETWORK_AI_URL_MAX] = {0};
    char ha_url[NETWORK_HA_URL_MAX] = {0};
    char ha_token[NETWORK_HA_TOKEN_MAX] = {0};

    bool has_ssid = form_get_value(body, "ssid", ssid, sizeof(ssid));
    bool has_password = form_get_value(body, "password", password, sizeof(password));
    bool has_ai = form_get_value(body, "ai_url", ai_url, sizeof(ai_url));
    bool has_ha_url = form_get_value(body, "ha_url", ha_url, sizeof(ha_url));
    bool has_ha_token = form_get_value(body, "ha_token", ha_token, sizeof(ha_token));
    free(body);

    char next_ssid[NETWORK_WIFI_SSID_MAX];
    char next_password[NETWORK_WIFI_PASSWORD_MAX];
    strlcpy(next_ssid, s_status.ssid, sizeof(next_ssid));
    strlcpy(next_password, s_wifi_password, sizeof(next_password));

    if (has_ssid && ssid[0] != '\0') strlcpy(next_ssid, ssid, sizeof(next_ssid));
    if (has_password && password[0] != '\0') {
        strlcpy(next_password, password, sizeof(next_password));
    }

    network_backend_config_t next_backend = s_backend;
    if (has_ai) strlcpy(next_backend.ai_url, ai_url, sizeof(next_backend.ai_url));
    if (has_ha_url) strlcpy(next_backend.ha_url, ha_url, sizeof(next_backend.ha_url));
    if (has_ha_token && ha_token[0] != '\0') {
        strlcpy(next_backend.ha_token, ha_token, sizeof(next_backend.ha_token));
    }

    esp_err_t backend_err = network_service_set_backend_config(&next_backend);
    esp_err_t wifi_err = ESP_OK;

    if (next_ssid[0] != '\0') {
        (void)network_service_set_wifi_enabled(true);
        wifi_err = network_service_set_wifi_credentials(next_ssid, next_password);
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    if (backend_err != ESP_OK || wifi_err != ESP_OK) {
        return httpd_resp_send(
            req,
            "<!doctype html><meta name='viewport' content='width=device-width'>"
            "<body style='font-family:sans-serif;background:#111;color:#fff;padding:30px'>"
            "<h2>保存失败</h2><p>请返回重新检查配置。</p></body>",
            HTTPD_RESP_USE_STRLEN);
    }

    return httpd_resp_send(
        req,
        "<!doctype html><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width'>"
        "<body style='font-family:sans-serif;background:#111;color:#fff;padding:30px'>"
        "<h2>配置已保存</h2><p>设备正在连接 Wi-Fi。连接成功后配置热点会自动关闭。</p>"
        "</body>",
        HTTPD_RESP_USE_STRLEN);
}

static esp_err_t start_http_server(void)
{
    if (s_httpd != NULL) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 6;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) return err;

    const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = setup_page_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = setup_save_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = setup_page_handler,
        .user_ctx = NULL,
    };

    err = httpd_register_uri_handler(s_httpd, &root);
    if (err == ESP_OK) err = httpd_register_uri_handler(s_httpd, &save);
    if (err == ESP_OK) err = httpd_register_uri_handler(s_httpd, &captive);

    if (err != ESP_OK) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    return err;
}

static void stop_http_server(void)
{
    if (s_httpd != NULL) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
}

static void dns_task(void *arg)
{
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS captive socket create failed");
        s_dns_running = false;
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    s_dns_socket = sock;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS captive bind failed");
        close(sock);
        s_dns_socket = -1;
        s_dns_running = false;
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    uint8_t packet[512];

    while (s_dns_running) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        int len = recvfrom(sock, packet, sizeof(packet) - 16, 0,
                           (struct sockaddr *)&client, &client_len);
        if (len < 12) continue;

        /* Standard one-question DNS response: preserve the original question
         * and append one A record pointing every hostname at 192.168.4.1. */
        packet[2] = 0x81;
        packet[3] = 0x80;
        packet[6] = 0x00;
        packet[7] = 0x01;
        packet[8] = packet[9] = packet[10] = packet[11] = 0x00;

        uint8_t answer[] = {
            0xC0, 0x0C,             /* name pointer */
            0x00, 0x01,             /* A */
            0x00, 0x01,             /* IN */
            0x00, 0x00, 0x00, 0x00,/* TTL */
            0x00, 0x04,             /* data len */
            192, 168, 4, 1
        };

        if ((size_t)len + sizeof(answer) <= sizeof(packet)) {
            memcpy(packet + len, answer, sizeof(answer));
            sendto(sock, packet, len + sizeof(answer), 0,
                   (struct sockaddr *)&client, client_len);
        }
    }

    close(sock);
    s_dns_socket = -1;
    s_dns_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t start_dns_server(void)
{
    if (s_dns_running) return ESP_OK;
    s_dns_running = true;

    if (xTaskCreate(dns_task, "setup_dns", 3072, NULL, 3, &s_dns_task) != pdPASS) {
        s_dns_running = false;
        s_dns_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void stop_dns_server(void)
{
    s_dns_running = false;
    if (s_dns_socket >= 0) {
        shutdown(s_dns_socket, SHUT_RDWR);
    }
}

static void delayed_portal_stop_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(5000));
    (void)network_service_stop_setup_portal();
    s_portal_stop_scheduled = false;
    vTaskDelete(NULL);
}

static void schedule_portal_stop(void)
{
    if (!s_setup_active || s_portal_stop_scheduled) return;
    s_portal_stop_scheduled = true;
    if (xTaskCreate(delayed_portal_stop_task, "portal_stop", 3072,
                    NULL, 2, NULL) != pdPASS) {
        s_portal_stop_scheduled = false;
    }
}

static void event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_status.connected = false;
        s_status.ip[0] = '\0';
        if (s_wifi_enabled && s_status.configured) {
            (void)esp_wifi_connect();
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        snprintf(s_status.ip, sizeof(s_status.ip),
                 IPSTR, IP2STR(&event->ip_info.ip));
        s_status.connected = true;
        refresh_link_metadata();
        start_time_sync();
        schedule_portal_stop();
        ESP_LOGI(TAG, "Wi-Fi connected: %s, ip=%s",
                 s_status.ssid, s_status.ip);
    }
}

esp_err_t network_service_init(void)
{
    if (s_initialized) return ESP_OK;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    err = load_persistent_config();
    if (err != ESP_OK) return err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_sta_netif == NULL || s_ap_netif == NULL) return ESP_FAIL;

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) return err;

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     event_handler, NULL);
    if (err != ESP_OK) return err;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     event_handler, NULL);
    if (err != ESP_OK) return err;

    s_initialized = true;
    s_status.initialized = true;
    s_status.enabled = s_wifi_enabled;
    refresh_link_metadata();

    if (!s_status.configured) {
        err = network_service_start_setup_portal();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "setup portal start failed: %s", esp_err_to_name(err));
        }
        return ESP_OK;
    }

    if (s_wifi_enabled) {
        err = ensure_wifi_runtime();
        if (err != ESP_OK) return err;
        err = apply_wifi_config();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "initial Wi-Fi connect failed: %s", esp_err_to_name(err));
        }
    }

    return ESP_OK;
}

esp_err_t network_service_set_wifi_enabled(bool enabled)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(nvs, KEY_WIFI_ENABLED, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK) return err;

    s_wifi_enabled = enabled;
    s_status.enabled = enabled;
    s_status.connected = false;
    s_status.ip[0] = '\0';

    if (!s_initialized) return ESP_OK;

    if (!enabled) {
        (void)esp_wifi_disconnect();
    }

    err = ensure_wifi_runtime();
    if (err != ESP_OK) return err;

    return (enabled && s_status.configured) ? apply_wifi_config() : ESP_OK;
}

esp_err_t network_service_set_wifi_credentials(const char *ssid,
                                               const char *password)
{
    if (ssid == NULL || password == NULL) return ESP_ERR_INVALID_ARG;
    if (strlen(ssid) >= NETWORK_WIFI_SSID_MAX ||
        strlen(password) >= NETWORK_WIFI_PASSWORD_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = save_string_pair(KEY_WIFI_SSID, ssid,
                                     KEY_WIFI_PASS, password);
    if (err != ESP_OK) return err;

    strlcpy(s_status.ssid, ssid, sizeof(s_status.ssid));
    strlcpy(s_wifi_password, password, sizeof(s_wifi_password));
    s_status.configured = s_status.ssid[0] != '\0';
    s_status.connected = false;

    if (!s_initialized) return ESP_OK;

    (void)esp_wifi_disconnect();
    return apply_wifi_config();
}

esp_err_t network_service_get_wifi_credentials(char *ssid,
                                               size_t ssid_size,
                                               char *password,
                                               size_t password_size)
{
    if (ssid == NULL || password == NULL ||
        ssid_size == 0 || password_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy(ssid, s_status.ssid, ssid_size);
    strlcpy(password, s_wifi_password, password_size);
    return ESP_OK;
}

void network_service_get_wifi_status(network_wifi_status_t *status)
{
    if (status == NULL) return;
    s_status.enabled = s_wifi_enabled;
    *status = s_status;

    if (s_sntp_started) {
        time_t now = time(NULL);
        if (now > 1700000000) {
            status->time_synced = true;
            s_status.time_synced = true;
        }
    }
}

esp_err_t network_service_start_setup_portal(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (s_setup_active) return ESP_OK;

    s_setup_active = true;

    esp_err_t err = ensure_wifi_runtime();
    if (err != ESP_OK) {
        s_setup_active = false;
        return err;
    }

    err = start_http_server();
    if (err != ESP_OK) {
        s_setup_active = false;
        (void)ensure_wifi_runtime();
        return err;
    }

    err = start_dns_server();
    if (err != ESP_OK) {
        stop_http_server();
        s_setup_active = false;
        (void)ensure_wifi_runtime();
        return err;
    }

    ESP_LOGI(TAG, "setup portal active: ssid=%s url=%s",
             SETUP_SSID, SETUP_URL);
    return ESP_OK;
}

esp_err_t network_service_stop_setup_portal(void)
{
    if (!s_initialized || !s_setup_active) return ESP_OK;

    stop_dns_server();
    stop_http_server();
    s_setup_active = false;

    esp_err_t err = ensure_wifi_runtime();
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "setup portal stopped");
    return ESP_OK;
}

bool network_service_setup_portal_active(void)
{
    return s_setup_active;
}

const char *network_service_setup_ssid(void)
{
    return SETUP_SSID;
}

const char *network_service_setup_url(void)
{
    return SETUP_URL;
}

esp_err_t network_service_set_backend_config(const network_backend_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs, KEY_AI_URL, config->ai_url);
    if (err == ESP_OK) err = nvs_set_str(nvs, KEY_HA_URL, config->ha_url);
    if (err == ESP_OK) err = nvs_set_str(nvs, KEY_HA_TOKEN, config->ha_token);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK) return err;

    s_backend = *config;
    return ESP_OK;
}

esp_err_t network_service_get_backend_config(network_backend_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    *config = s_backend;
    return ESP_OK;
}
