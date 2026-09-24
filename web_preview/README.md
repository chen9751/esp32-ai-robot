# LVGL Web Preview
This target compiles the same `components/ui/ui_manager.c` used by the ESP32 project into WebAssembly with Emscripten.

- Fixed UI orientation: **640x172 landscape**
- Browser touch/mouse maps directly to LVGL pointer input
- No browser-side or UI-side 90-degree rotation logic
- UI pushes trigger GitHub Actions

For phone preview, rotate the phone itself to landscape. One-time repository setup: **Settings > Pages > Build and deployment > Source: GitHub Actions**.
