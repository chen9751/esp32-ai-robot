cmake_minimum_required(VERSION 3.16)
project(ai_robot_web_preview C)
include(FetchContent)
FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v9.2.2 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(lvgl)
# LVGL itself also needs the preview config while it is compiled.
target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(index
    main.c
    ../components/ui/ui_manager.c
    ../components/ui/ui_page_home.c
    ../components/ui/ui_page_standby.c
    ../components/ui/ui_assets.c
    ../components/ui/ui_asset_remote.c
    ../components/ui/ui_asset_music.c
    ../components/ui/ui_asset_light.c
    ../components/ui/ui_asset_devices.c
    ../components/ui/ui_asset_alarm.c
    ../components/ui/ui_asset_settings.c
)
target_include_directories(index PRIVATE . ../components/ui/include)
target_compile_definitions(index PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_link_libraries(index PRIVATE lvgl)
target_compile_options(index PRIVATE "-sUSE_SDL=2")
target_link_options(index PRIVATE "-sUSE_SDL=2" "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=67108864" "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
set_target_properties(index PROPERTIES SUFFIX ".html")
