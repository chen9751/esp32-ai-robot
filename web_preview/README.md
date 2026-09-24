# LVGL Web Preview
This target compiles the same `components/ui/ui_manager.c` used by the ESP32 project into WebAssembly with Emscripten.

- Main UI: 640x172 landscape
- Remote: 172x640 portrait
- Browser touch/mouse maps to LVGL pointer input
- UI pushes trigger GitHub Actions

One-time repository setup: **Settings > Pages > Build and deployment > Source: GitHub Actions**.
