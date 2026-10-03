#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_WIFI_SSID_MAX      33
#define NETWORK_WIFI_PASSWORD_MAX  65
#define NETWORK_AI_URL_MAX         128
#define NETWORK_HA_URL_MAX         128
#define NETWORK_HA_TOKEN_MAX       256

typedef struct {
    bool initialized;
    bool enabled;
    bool configured;
    bool connected;
    bool time_synced;
    char ssid[NETWORK_WIFI_SSID_MAX];
    char ip[16];
    char mac[18];
    char dns[16];
} network_wifi_status_t;

typedef struct {
    char ai_url[NETWORK_AI_URL_MAX];
    char ha_url[NETWORK_HA_URL_MAX];
    char ha_token[NETWORK_HA_TOKEN_MAX];
} network_backend_config_t;

esp_err_t network_service_init(void);

esp_err_t network_service_set_wifi_enabled(bool enabled);
esp_err_t network_service_set_wifi_credentials(const char *ssid,
                                               const char *password);
esp_err_t network_service_get_wifi_credentials(char *ssid,
                                               size_t ssid_size,
                                               char *password,
                                               size_t password_size);
void network_service_get_wifi_status(network_wifi_status_t *status);

esp_err_t network_service_set_backend_config(const network_backend_config_t *config);
esp_err_t network_service_get_backend_config(network_backend_config_t *config);

#ifdef __cplusplus
}
#endif
