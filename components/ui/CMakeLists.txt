set(UI_SRCS
    "ui_manager.c"
    "ui_page_home.c"
    "ui_page_feature.c"
    "ui_page_remote.c"
    "ui_remote_icons.c"
    "ui_page_music.c"
    "ui_page_lights.c"
    "ui_lights_icons.c"
    "ui_lights_extra_icons.c"
    "ui_page_devices.c"
    "ui_page_settings.c"
    "ui_system_icons.c"
    "ui_icon_robot2.c"
    "ui_page_standby.c"
    "ui_page_clock.c"
    "ui_assets.c"
    "ui_asset_remote.c"
    "ui_asset_music.c"
    "ui_asset_light.c"
    "ui_asset_devices.c"
    "ui_asset_settings.c"
    "ui_lights_labels.c"
    "ui_lights_label_living.c"
    "ui_lights_label_study.c"
    "ui_lights_labels_beds.c"
    "ui_lights_labels_extended.c"
    "ui_lights_labels_utility.c"
)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/ui_font_source_han_lights_18.c")
    list(APPEND UI_SRCS "ui_font_source_han_lights_18.c")
    set(UI_LIGHTS_HAS_SOURCE_HAN TRUE)
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/ui_font_source_han_devices_16.c" AND
   EXISTS "${CMAKE_CURRENT_LIST_DIR}/ui_font_source_han_devices_temp_84.c" AND
   EXISTS "${CMAKE_CURRENT_LIST_DIR}/ui_font_remix_devices_28.c" AND
   EXISTS "${CMAKE_CURRENT_LIST_DIR}/ui_font_remix_devices_56.c")
    list(APPEND UI_SRCS
        "ui_font_source_han_devices_16.c"
        "ui_font_source_han_devices_temp_84.c"
        "ui_font_remix_devices_28.c"
        "ui_font_remix_devices_56.c"
    )
    set(UI_DEVICES_HAS_FONTS TRUE)
endif()

idf_component_register(
    SRCS ${UI_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES lvgl board network bluetooth audio
)

if(UI_LIGHTS_HAS_SOURCE_HAN)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE UI_LIGHTS_HAS_SOURCE_HAN=1)
endif()

if(UI_DEVICES_HAS_FONTS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE UI_DEVICES_HAS_FONTS=1)
endif()
