# ESP32 AI Robot

ESP32-S3 client for a local AI robot, targeting Waveshare ESP32-S3-Touch-LCD-3.49 V2.

Initial stack: ESP-IDF 5.5.4 + LVGL 9. Hardware, UI, audio, networking and robot protocol are kept modular so the same UI can later be previewed on macOS.

## UI baseline

- Target UI resolution: **640 × 172**, landscape. Do not design against the panel's official portrait orientation.
- Main UI background: pure black unless a page explicitly needs otherwise.
- UI framework: LVGL 9.
- The current Web Preview compatibility baseline is **LVGL 9.2.2**. UI code must stay compatible with this baseline unless the project deliberately raises it and updates the preview/build checks at the same time.
- Keep layout, typography, icons and image assets independent from hardware code so the same UI can be used by the simulator/web preview.
- The Waveshare panel is physically 172 × 640. Landscape display rotation and the matching touch-coordinate transform belong in the hardware/display-input port, not inside individual UI pages.

## Typography and icon assets

The project has selected the following three open-source type/icon families as the default design sources. New UI work should reuse these instead of introducing unrelated fonts or ad-hoc pixel icons.

| Role | Selected family | Intended use |
| --- | --- | --- |
| Main UI text | **Source Han Sans CN Normal / 思源黑体 CN Normal** | Chinese UI text, labels, settings and normal interface copy |
| Retro / clock text | **Fusion Pixel Font / 缝合像素字体** | Clock, retro LCD/old-screen style numbers and selected status text |
| UI icons | **Remix Icon** | Navigation, remote, music, lights, devices, timer, alarm, settings and later system/device icons |

Source assets and LVGL-generated subsets are documented under `assets/fonts/README.md`.

### Embedded-font rule

The desktop font files are **design/source assets**, not a requirement to embed the complete font files into ESP32 firmware. For firmware builds, generate LVGL font subsets containing only the glyphs/icons and sizes actually required by the UI. This keeps Flash/RAM use under control while preserving a consistent visual system.

Do not replace the selected icon system with enlarged bitmap/pixel icons unless a page intentionally calls for a pixel-art style.

For small monochrome UI icons, compact A8 image subsets rasterized directly from the selected Remix Icon SVG source are also acceptable. They must retain the original Remix Icon geometry and be documented with their upstream icon filenames; do not redraw equivalent icons with one-off LVGL geometry.

## Settings and phone configuration model

The 640 × 172 device screen is intentionally **not** used as a full text-entry configuration console. Complex text entry is delegated to a phone-based Web configuration flow so the embedded UI can stay compact and touch-friendly.

### Device-side settings

Keep frequent, low-complexity controls local on the device:

- volume
- display brightness
- microphone or other simple on/off controls
- Wi-Fi on/off
- Bluetooth on/off
- status and diagnostic information

The current settings navigation uses six tabs:

`Sound | Display | Wi-Fi | Bluetooth | AI | System`

Sound and Display provide direct local controls. Wi-Fi and AI primarily act as status/configuration entry pages. Bluetooth and System remain available for later expansion.

### QR-based configuration entry

Configuration that requires typing, long URLs, passwords or other structured text should use a QR-based phone flow rather than an on-screen LVGL keyboard.

Current intended behavior:

- **Wi-Fi not configured / not connected**: show a QR configuration entry on the device.
- **Wi-Fi connected**: show SSID, connection state, IP, MAC and DNS, plus local `Disconnect` and `Forget` actions.
- **AI not configured**: show the same QR-based configuration entry pattern.
- **AI configured**: show current AI server information such as server address, port, model and online/offline state.

The QR shown in the current UI is only a visual placeholder. The real QR payload, local Web page and provisioning transport will be implemented later.

### Future Web configuration service

The planned phone configuration page will be a local Web interface hosted by or associated with the device. It is expected to handle settings that are awkward to enter on a 640 × 172 touch display, especially:

- Wi-Fi SSID and password
- AI server address / hostname
- AI server port
- model selection or model identifier
- future text-based integration settings such as Home Assistant addresses or tokens

The embedded LVGL layer must stay independent from ESP-IDF networking logic. UI code should display state and emit user actions; networking/provisioning code should own scanning, association, credential persistence, connection state and actual QR/Web endpoint generation.

## UI page architecture

UI pages are intentionally split into separate modules so the project does not grow into one large UI source file.

- `ui_manager.c`: page routing and the global 60-second idle timeout.
- `ui_page_home.c`: seven-function HOME carousel: Remote, Music, Lights, Devices, Timer, Alarm and Settings.
- `ui_page_feature.c`: shared function-page shell, global left-side back gesture/animation and feature routing.
- `ui_page_music.c`: compact music transport page with metadata, seek/progress display, previous/play-pause/next controls and a two-way `TV | Speaker` target selector. It emits UI actions only; Debian playback control, Bluetooth HID and playback-state synchronization stay outside the LVGL page.
- `ui_page_lights.c`: horizontally scrollable lighting control page. It owns the eight room/light tiles, local on/off presentation and the brightness/color-temperature/RGB adjustment UI while remaining independent from Home Assistant/network business logic.
- `ui_page_devices.c`: four full-screen horizontally paged device placeholders in the fixed order Air Conditioner, Curtain, Bath Heater and Drying Rack. Each page occupies the complete 640 × 172 canvas, uses page snapping and loops continuously in both horizontal directions; device/HA business logic remains outside the UI module.
- `ui_page_timer.c`: countdown timer UI and timer state. Hours/minutes/seconds are adjusted with vertical drag, the maximum duration is 12:00:00, and the countdown keeps running independently from page lifetime.
- `ui_page_settings.c`: settings page content and controls.
- `ui_system_icons.c`: shared system/device icon assets; current settings icons are compact A8 subsets generated from Remix Icon sources.
- `ui_page_standby.c`: standby gesture/navigation controller only.
- `ui_page_clock.c`: standby clock content.
- `ui_page_weather.c`: weather page (placeholder until designed).
- `ui_page_calendar.c`: calendar page (placeholder until designed).

Music behavior is defined as follows: the page has no album artwork and no shuffle, repeat or volume controls. The upper-right selector switches only between `TV` and `Speaker`; there is no additional "control target" label. `TV` is reserved for direct ESP32 Bluetooth-HID media control of the television, while `Speaker` is reserved for commands sent to the Debian playback service that outputs to the paired iPad mini / CM220 speaker path. The LVGL module stores presentation state, exposes metadata/playback setters and emits target/transport/seek actions; transport-specific code must consume those actions elsewhere.

Lights behavior is defined as follows: tapping a room/device tile toggles that light's local on/off presentation. Normal lights expose brightness and color-temperature controls; RGB-capable lights additionally expose a color control; switch-only lights keep the same card footprint without fake adjustment controls. Brightness is adjusted in 10% steps. Color temperature currently spans 2500 K to 6500 K in 400 K steps. RGB adjustment uses two sliders: hue and saturation, both in 10% steps; saturation is rendered from white to the currently selected hue. Entering an adjustment temporarily replaces the card title/icon area, tapping the same active control exits, switching to another control changes adjustment mode directly, and 30 seconds without adjustment returns the card to its default presentation. These values are currently local UI state only; later Home Assistant/network integration must consume/refresh this state outside the page rather than embedding transport logic into the UI module.

Devices behavior is currently placeholder-only. Entering Devices always starts on the Air Conditioner page. Horizontal swipes move one full 640 × 172 page at a time through Air Conditioner, Curtain, Bath Heater and Drying Rack, then continue directly back to Air Conditioner; swiping the opposite direction from Air Conditioner continues directly to Drying Rack. The shared feature shell keeps the existing left-side back interaction above the device pager. No device cards, controls or Home Assistant transport logic are implemented yet.

Timer behavior is defined as follows: the page contains no text labels. It shows only `HH:MM:SS` and circular icon controls. In the setting state, the single large Play control is green. Starting replaces it with two smaller circular controls: an orange Pause control above and a red Stop control below. Pausing freezes the remaining time and changes the upper control to an orange Play/Resume icon. Stop cancels the active countdown and restores the most recently configured duration. When the countdown reaches zero it remains at `00:00:00`; the upper action can immediately replay the last configured duration, while Stop returns to the editable setting state with that duration restored. A zero-duration timer does not start. Button backgrounds are translucent while their icons remain fully colored.

Standby behavior is fixed as follows: after 60 seconds without input, an inactive page returns to the clock. **While a countdown timer is actively running, the automatic 60-second return to the standby clock is suppressed.** Pause, Stop or countdown completion restores the normal idle behavior. The clock/weather/calendar pages loop horizontally. A short tap from any standby page opens HOME. Upward swiping from standby moves the standby page itself upward with the finger, revealing a stationary HOME page underneath. From HOME, a downward swipe brings the clock in from above to cover HOME. These are intentionally asymmetric inverse transitions. These vertical transitions never return to a previously active function page.
