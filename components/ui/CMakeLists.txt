idf_component_register(
    SRCS
        "ui_manager.c"
        "ui_assets.c"
        "ui_asset_remote.c"
        "ui_asset_music.c"
        "ui_asset_light.c"
        "ui_asset_devices.c"
        "ui_asset_alarm.c"
        "ui_asset_settings.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)
