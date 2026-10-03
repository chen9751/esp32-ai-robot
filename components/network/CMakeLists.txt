idf_component_register(
    SRCS "network_service.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_netif esp_wifi esp_http_server nvs_flash lwip
)
