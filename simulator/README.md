# macOS UI preview

This preview runs the same `components/ui/ui_manager.c` UI on macOS at the hardware-correct landscape resolution of **640x172**, displayed at 2x scale (1280x344).

The Waveshare panel is natively 172x640; the project's primary landscape UI is therefore 640x172. The earlier 320x172 prototype size is retired.

## First-time setup

Install Homebrew packages:

    brew install cmake pkg-config sdl2

The ESP-IDF component manager downloads LVGL into `managed_components` after the first firmware configure/build. If it is not present, use the ESP-IDF environment first:

    . ~/esp/esp-idf/export.sh
    idf.py reconfigure

## Build preview

    cd simulator
    cmake -S . -B build
    cmake --build build -j
    ./build/ai_robot_ui_preview

Mouse click/drag simulates touch. The current UI widgets still retain their v1 coordinates; this change corrects the display canvas first, before the UI is redesigned for the full 640-pixel width.
