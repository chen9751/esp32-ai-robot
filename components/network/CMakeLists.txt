idf_component_register(
    SRCS "network_service.c" "ha_lights.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_netif esp_wifi nvs_flash fatfs sdmmc json driver lwip esp_http_client
)
