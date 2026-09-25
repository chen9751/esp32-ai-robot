# ESP32 AI Robot

ESP32-S3 client for a local AI robot, targeting Waveshare ESP32-S3-Touch-LCD-3.49 V2.

Initial stack: ESP-IDF 5.5.4 + LVGL 9. Hardware, UI, audio, networking and robot protocol are kept modular so the same UI can later be previewed on macOS.

## UI baseline

- Target UI resolution: **640 × 172**, landscape. Do not design against the panel's official portrait orientation.
- Main UI background: pure black unless a page explicitly needs otherwise.
- UI framework: LVGL 9.
- Keep layout, typography, icons and image assets independent from hardware code so the same UI can be used by the simulator/web preview.

## Typography and icon assets

The project has selected the following three open-source type/icon families as the default design sources. New UI work should reuse these instead of introducing unrelated fonts or ad-hoc pixel icons.

| Role | Selected family | Intended use |
| --- | --- | --- |
| Main UI text | **Source Han Sans CN Normal / 思源黑体 CN Normal** | Chinese UI text, labels, settings and normal interface copy |
| Retro / clock text | **Fusion Pixel Font / 缝合像素字体** | Clock, retro LCD/old-screen style numbers and selected status text |
| UI icons | **Remix Icon** | Navigation, remote, music, lights, devices, alarm, settings and later system/device icons |

Source assets and LVGL-generated subsets are documented under `assets/fonts/README.md`.

### Embedded-font rule

The desktop font files are **design/source assets**, not a requirement to embed the complete font files into ESP32 firmware. For firmware builds, generate LVGL font subsets containing only the glyphs/icons and sizes actually required by the UI. This keeps Flash/RAM use under control while preserving a consistent visual system.

Do not replace the selected icon system with enlarged bitmap/pixel icons unless a page intentionally calls for a pixel-art style.


## UI page architecture

UI pages are intentionally split into separate modules so the project does not grow into one large UI source file.

- `ui_manager.c`: page routing and the global 60-second idle timeout.
- `ui_page_home.c`: six-function HOME carousel.
- `ui_page_standby.c`: standby gesture/navigation controller only.
- `ui_page_clock.c`: standby clock content.
- `ui_page_weather.c`: weather page (placeholder until designed).
- `ui_page_calendar.c`: calendar page (placeholder until designed).

Standby behavior is fixed as follows: after 60 seconds without input, any active page returns to the clock. The clock/weather/calendar pages loop horizontally. A short tap from any standby page opens HOME. Upward swiping from standby opens HOME with an interactive finger-following cover transition. From HOME, a downward swipe returns to the clock with the reverse interactive cover transition. These vertical transitions never return to a previously active function page.
