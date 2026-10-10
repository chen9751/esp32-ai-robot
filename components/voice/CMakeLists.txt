idf_component_register(
    SRCS "voice_wakeup.c" "voice_bridge.c"
    INCLUDE_DIRS "include"
    REQUIRES audio network ui esp-sr esp_http_client
)
