cmake_minimum_required(VERSION 3.16)
project(ai_robot_ui_preview C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_subdirectory(../managed_components/lvgl__lvgl lvgl)

add_executable(ai_robot_ui_preview
    main.c
    ../components/ui/ui_manager.c
    ../components/ui/ui_page_home.c
    ../components/ui/ui_page_standby.c
    ../components/ui/ui_page_clock.c
    ../components/ui/ui_page_weather.c
    ../components/ui/ui_page_calendar.c
    ../components/ui/ui_assets.c
    ../components/ui/ui_asset_remote.c
    ../components/ui/ui_asset_music.c
    ../components/ui/ui_asset_light.c
    ../components/ui/ui_asset_devices.c
    ../components/ui/ui_asset_alarm.c
    ../components/ui/ui_asset_settings.c
)

target_include_directories(ai_robot_ui_preview PRIVATE
    ../components/ui/include
)

target_link_libraries(ai_robot_ui_preview PRIVATE
    lvgl
    PkgConfig::SDL2
)

target_compile_definitions(ai_robot_ui_preview PRIVATE LV_CONF_INCLUDE_SIMPLE)
