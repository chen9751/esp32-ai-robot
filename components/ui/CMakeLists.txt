set(UI_SRCS
    "ui_manager.c"
    "ui_page_home.c"
    "ui_page_feature.c"
    "ui_page_lights.c"
    "ui_page_timer.c"
    "ui_page_alarm.c"
    "ui_page_settings.c"
    "ui_system_icons.c"
    "ui_icon_robot2.c"
    "ui_page_standby.c"
    "ui_page_clock.c"
    "ui_page_weather.c"
    "ui_page_calendar.c"
    "ui_assets.c"
    "ui_asset_remote.c"
    "ui_asset_music.c"
    "ui_asset_light.c"
    "ui_asset_devices.c"
    "ui_asset_timer.c"
    "ui_asset_alarm.c"
    "ui_asset_settings.c"
    "ui_lights_labels.c"
    "ui_lights_label_living.c"
    "ui_lights_label_study.c"
    "ui_lights_labels_beds.c"
    "ui_lights_labels_extended.c"
    "ui_lights_labels_utility.c"
)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/ui_font_source_han_alarm_16.c")
    list(APPEND UI_SRCS "ui_font_source_han_alarm_16.c")
    set(UI_ALARM_HAS_SOURCE_HAN TRUE)
endif()

idf_component_register(
    SRCS ${UI_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)

if(UI_ALARM_HAS_SOURCE_HAN)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE UI_ALARM_HAS_SOURCE_HAN=1)
endif()
