#include "network_service.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdmmc_cmd.h"
#include "lwip/inet.h"

#define TF_MOUNT_POINT "/sdcard"
#define TF_CONFIG_PATH TF_MOUNT_POINT "/config.json"
#define TF_CONFIG_MAX_BYTES 4096

/* Verified against Waveshare ESP32-S3-Touch-LCD-3.49-V2 ESP-IDF/04_SD_Card. */
#define TF_SDMMC_CMD GPIO_NUM_39
#define TF_SDMMC_D0  GPIO_NUM_40
#define TF_SDMMC_CLK GPIO_NUM_41

static const char *TAG = "network";
static network_wifi_status_t s_status;
static network_backend_config_t s_backend;
static char s_password[NETWORK_WIFI_PASSWORD_MAX];
static const char *s_config_status = "TF NOT READ";
static bool s_initialized;
static bool s_sntp_started;
static sdmmc_card_t *s_card;

static bool copy_json_string(const cJSON *object, const char *key,
                             char *dst, size_t capacity, bool required)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item) {
        if (required) return false;
        dst[0] = '\0';
        return true;
    }
    if (!cJSON_IsString(item) || !item->valuestring) return false;
    size_t len = strlen(item->valuestring);
    if (len >= capacity || (required && len == 0)) return false;
    memcpy(dst, item->valuestring, len + 1);
    return true;
}

static esp_err_t mount_tf_card(void)
{
    if (s_card) return ESP_OK;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    /* Configuration is tiny: use a conservative clock for startup reliability. */
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.cmd = TF_SDMMC_CMD;
    slot.clk = TF_SDMMC_CLK;
    slot.d0 = TF_SDMMC_D0;

    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false, /* Never erase user configuration. */
        .max_files = 2,
        .allocation_unit_size = 16 * 1024,
    };
    return esp_vfs_fat_sdmmc_mount(TF_MOUNT_POINT, &host, &slot, &mount, &s_card);
}

static esp_err_t load_tf_config(void)
{
    esp_err_t err = mount_tf_card();
    if (err != ESP_OK) {
        s_config_status = "TF MOUNT ERROR";
        ESP_LOGW(TAG, "TF mount failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "TF card mounted; loading configuration");

    errno = 0;
    FILE *file = fopen(TF_CONFIG_PATH, "rb");
    if (!file) {
        s_config_status = "CONFIG MISSING";
        int saved_errno = errno;
        ESP_LOGW(TAG, "Cannot open %s: errno=%d (%s)",
                 TF_CONFIG_PATH, saved_errno, strerror(saved_errno));
        return ESP_ERR_NOT_FOUND;
    }

    /* app_main() has a small stack: keep the JSON input on the heap. */
    char *buffer = malloc(TF_CONFIG_MAX_BYTES + 1);
    if (!buffer) {
        fclose(file);
        s_config_status = "NO MEMORY";
        return ESP_ERR_NO_MEM;
    }
    size_t size = fread(buffer, 1, TF_CONFIG_MAX_BYTES + 1, file);
    bool io_error = ferror(file);
    bool too_large = size > TF_CONFIG_MAX_BYTES;
    fclose(file);
    if (io_error || too_large || size == 0) {
        free(buffer);
        s_config_status = "CONFIG INVALID";
        return ESP_ERR_INVALID_SIZE;
    }
    buffer[size] = '\0';

    cJSON *root = cJSON_ParseWithLength(buffer, size);
    free(buffer);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        s_config_status = "JSON INVALID";
        ESP_LOGW(TAG, "Malformed %s", TF_CONFIG_PATH);
        return ESP_ERR_INVALID_ARG;
    }

    network_wifi_status_t wifi = {0};
    network_backend_config_t backend = {0};
    char password[NETWORK_WIFI_PASSWORD_MAX] = {0};
    const cJSON *wifi_obj = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    const cJSON *ai_obj = cJSON_GetObjectItemCaseSensitive(root, "ai");
    const cJSON *ha_obj = cJSON_GetObjectItemCaseSensitive(root, "ha");

    bool valid = cJSON_IsObject(wifi_obj) && cJSON_IsObject(ai_obj) &&
        copy_json_string(wifi_obj, "ssid", wifi.ssid, sizeof(wifi.ssid), true) &&
        copy_json_string(wifi_obj, "password", password, sizeof(password), false) &&
        copy_json_string(ai_obj, "url", backend.ai_url,
                         sizeof(backend.ai_url), false);
    if (ha_obj) {
        valid = valid && cJSON_IsObject(ha_obj) &&
            copy_json_string(ha_obj, "url", backend.ha_url,
                             sizeof(backend.ha_url), false) &&
            copy_json_string(ha_obj, "token", backend.ha_token,
                             sizeof(backend.ha_token), false);
    }
    cJSON_Delete(root);

    if (!valid) {
        s_config_status = "CONFIG INVALID";
        ESP_LOGW(TAG, "Invalid TF config fields (check string lengths and types)");
        return ESP_ERR_INVALID_ARG;
    }

    /* Commit only after all fields validate: no partially applied credentials. */
    s_status = wifi;
    s_status.enabled = true;
    s_status.configured = true;
    s_backend = backend;
    memcpy(s_password, password, sizeof(s_password));
    s_config_status = "CONFIG OK";
    ESP_LOGI(TAG, "TF config loaded: Wi-Fi SSID=%s; AI=%s; HA=%s",
             s_status.ssid, s_backend.ai_url[0] ? "configured" : "not set",
             s_backend.ha_url[0] ? "configured" : "not set");
    return ESP_OK;
}

static void on_time_synced(struct timeval *tv)
{
    (void)tv;
    s_status.time_synced = true;
    ESP_LOGI(TAG, "SNTP synchronized");
}

static void start_time_sync(void)
{
    if (s_sntp_started) return;
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = on_time_synced;
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err == ESP_OK) s_sntp_started = true;
    else ESP_LOGW(TAG, "SNTP init failed: %s", esp_err_to_name(err));
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_status.configured) (void)esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_status.connected = false;
        s_status.ip[0] = '\0';
        if (s_status.configured) (void)esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *ip = (const ip_event_got_ip_t *)data;
        snprintf(s_status.ip, sizeof(s_status.ip), IPSTR, IP2STR(&ip->ip_info.ip));
        s_status.connected = true;
        uint8_t mac[6] = {0};
        if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
            snprintf(s_status.mac, sizeof(s_status.mac),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
        start_time_sync();
        ESP_LOGI(TAG, "Wi-Fi connected, IP=%s", s_status.ip);
    }
}

esp_err_t network_service_init(void)
{
    if (s_initialized) return ESP_OK;

    /* TF card is the ONLY source for user Wi-Fi and AI configuration.
     * Missing/broken media is a nonfatal state; never start a setup AP. */
    esp_err_t config_err = load_tf_config();
    if (config_err != ESP_OK) return ESP_OK;

    esp_err_t err = nvs_flash_init(); /* ESP-IDF Wi-Fi driver internal NVS only. */
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (!netif) return ESP_ERR_NO_MEM;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 4;
    cfg.dynamic_rx_buf_num = 12;
    cfg.dynamic_tx_buf_num = 12;
    cfg.rx_mgmt_buf_num = 3;
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) return err;
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     wifi_event_handler, NULL);
    if (err != ESP_OK) return err;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     wifi_event_handler, NULL);
    if (err != ESP_OK) return err;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;

    wifi_config_t connection = {0};
    strlcpy((char *)connection.sta.ssid, s_status.ssid, sizeof(connection.sta.ssid));
    strlcpy((char *)connection.sta.password, s_password, sizeof(connection.sta.password));
    connection.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    connection.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    err = esp_wifi_set_config(WIFI_IF_STA, &connection);
    if (err != ESP_OK) return err;

    s_status.initialized = true;
    s_initialized = true;
    return esp_wifi_start();
}

void network_service_get_wifi_status(network_wifi_status_t *status)
{
    if (!status) return;
    *status = s_status;
    if (s_sntp_started && time(NULL) > 1700000000) {
        status->time_synced = true;
    }
}

esp_err_t network_service_get_backend_config(network_backend_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    *config = s_backend;
    return ESP_OK;
}

const char *network_service_config_status(void)
{
    return s_config_status;
}
