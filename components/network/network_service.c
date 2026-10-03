#include "network_service.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
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

static bool s_initialized = false;
static bool s_wifi_enabled = true;
static bool s_sntp_started = false;
static esp_netif_t *s_sta_netif = NULL;
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

static void start_time_sync(void)
{
    if (s_sntp_started) return;

    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = NULL;
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
            esp_wifi_connect();
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
        ESP_LOGI(TAG, "Wi-Fi connected: %s, ip=%s",
                 s_status.ssid, s_status.ip);
    }
}

static esp_err_t apply_wifi_config(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!s_wifi_enabled) return ESP_OK;

    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, s_status.ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, s_wifi_password, sizeof(cfg.sta.password));
    cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) return err;

    s_status.configured = s_status.ssid[0] != '\0';
    if (!s_status.configured) {
        esp_wifi_disconnect();
        s_status.connected = false;
        return ESP_OK;
    }

    return esp_wifi_connect();
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
    if (s_sta_netif == NULL) return ESP_FAIL;

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) return err;

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     event_handler, NULL);
    if (err != ESP_OK) return err;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     event_handler, NULL);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;

    if (s_wifi_enabled) {
        err = esp_wifi_start();
        if (err != ESP_OK) return err;
    }

    s_initialized = true;
    s_status.initialized = true;
    refresh_link_metadata();

    if (s_wifi_enabled && s_status.configured) {
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

    if (s_wifi_enabled == enabled) {
        s_status.enabled = enabled;
        return ESP_OK;
    }

    s_wifi_enabled = enabled;
    s_status.enabled = enabled;
    s_status.connected = false;
    s_status.ip[0] = '\0';

    if (!s_initialized) return ESP_OK;

    if (!enabled) {
        (void)esp_wifi_disconnect();
        return esp_wifi_stop();
    }

    err = esp_wifi_start();
    if (err != ESP_OK) return err;
    return s_status.configured ? apply_wifi_config() : ESP_OK;
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
    esp_wifi_disconnect();
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
