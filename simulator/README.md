# macOS UI preview

This preview runs the same `components/ui/ui_manager.c` UI on macOS at logical 320x172, scaled 3x.

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

Mouse click/drag simulates touch, so buttons, sliders and swipe gestures can be tested without flashing the board.
