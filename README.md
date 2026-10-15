# ESP32 AI Robot

ESP32-S3 client for a local AI robot, targeting Waveshare ESP32-S3-Touch-LCD-3.49 V2.

Initial stack: ESP-IDF 5.5.4 + LVGL 9. Hardware, UI, audio, networking and robot protocol are kept modular so the same UI can later be previewed on macOS.

## UI baseline

- Target UI resolution: **640 × 172**, landscape. Do not design against the panel's official portrait orientation.
- Physical mounting baseline: **speaker down, BOOT/custom key bottom-left, TF slot top-left**. The board driver maps the frame and touch coordinates to this orientation; IMU interpretation must follow the same baseline.
- Rotation mapping (factory mount): logical display `640×172` → native `172×640` using `native_x=logical_y`, `native_y=639-logical_x`; the touchscreen applies the inverse. The QMI8658 board orientation mapping and Remote icon tilt use this same baseline. Remote icons reset to upright when the page is rebuilt, then follow the IMU; UI button geometry and touch actions do not rotate.
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
| UI icons | **Remix Icon** | Navigation, remote, music, lights, devices, settings and later system/device icons |

Source assets and LVGL-generated subsets are documented under `assets/fonts/README.md`.

### Embedded-font rule

The desktop font files are **design/source assets**, not a requirement to embed the complete font files into ESP32 firmware. For firmware builds, generate LVGL font subsets containing only the glyphs/icons and sizes actually required by the UI. This keeps Flash/RAM use under control while preserving a consistent visual system.

Do not replace the selected icon system with enlarged bitmap/pixel icons unless a page intentionally calls for a pixel-art style.

For small monochrome UI icons, compact A8 image subsets rasterized directly from the selected Remix Icon SVG source are also acceptable. They must retain the original Remix Icon geometry and be documented with their upstream icon filenames; do not redraw equivalent icons with one-off LVGL geometry.

## Settings and TF card configuration

The device only reads configuration from the TF card; no Wi-Fi setup hotspot,
HTTP/DNS provisioning server or interactive Wi-Fi / AI editor is provided.

- See [tf_card/README.md](tf_card/README.md) and [tf_card/config.json](tf_card/config.json).
- Copy `tf_card/config.json` to the **root** of a FAT32-formatted TF card.
- Boot-time SDMMC uses Waveshare V2 CMD=GPIO39, D0=GPIO40, CLK=GPIO41 (1-bit).
- The firmware mounts `/sdcard` and reads `/sdcard/config.json`.
- Wi-Fi / AI settings are read-only status views; edits require reboot.
- Missing or invalid config is nonfatal and shown in Wi-Fi settings.
- The tab order is `Sound | Display | Bluetooth | Wi-Fi | AI | System`.
- The AI URL is configuration/status metadata, not an implemented connection probe.

## UI page architecture

UI pages are intentionally split into separate modules so the project does not grow into one large UI source file.

- `ui_manager.c`: page routing and the global 60-second idle timeout.
- `ui_page_home.c`: fixed four-item HOME (Remote, Music, Lights, Devices), 640 × 172 with no horizontal scroll. Swipe up for Settings; swipe down for standby.
- `ui_page_feature.c`: shared four-feature shell and consistent 180 ms full-width slide-in / right-drag exit. Settings is not a feature page.
- `ui_page_music.c`: compact music transport page with metadata, seek/progress display, previous/play-pause/next controls and a two-way `TV | Speaker` target selector. It emits UI actions only; Debian playback control, Bluetooth HID and playback-state synchronization stay outside the LVGL page.
- `ui_page_lights.c`: horizontally scrollable lighting control page. It owns the eight room/light tiles, local on/off presentation and the brightness/color-temperature/RGB adjustment UI while remaining independent from Home Assistant/network business logic.
- `ui_page_devices.c`: four full-screen horizontally paged device views in the fixed order Air Conditioner, Curtain, Bath Heater and Drying Rack. Each page occupies the complete 640 × 172 canvas, uses page snapping and loops continuously in both horizontal directions; device/HA business logic remains outside the UI module.
- `ui_page_settings.c`: settings page content and controls, shown as a vertical finger-following overlay; downward swipe returns HOME. Dragging sliders does not dismiss Settings.
- `ui_system_icons.c`: shared system/device icon assets; current settings icons are compact A8 subsets generated from Remix Icon sources.
- `ui_page_standby.c`: standby gesture/navigation controller only.
- `ui_page_clock.c`: standby clock content.
- **Implementation note (f760bcc lineage):** `ui_page_clock.c` still renders digits from a compact 5×7 table using LVGL rectangles, despite Fusion Pixel being the selected design-source font. Do not remove that code until an actual replacement is integrated and verified. Font design policy does not prove that the firmware is already using the font.
- Standby currently contains the clock only; obsolete weather/calendar carousel events have been removed.

Music behavior is defined as follows: the page has no album artwork and no shuffle, repeat or volume controls. The upper-right selector switches only between `TV` and `Speaker`; there is no additional "control target" label. `TV` is reserved for direct ESP32 Bluetooth-HID media control of the television, while `Speaker` is reserved for commands sent to the Debian playback service that outputs to the paired iPad mini / CM220 speaker path. The LVGL module stores presentation state, exposes metadata/playback setters and emits target/transport/seek actions; transport-specific code must consume those actions elsewhere.

Lights behavior is defined as follows: tapping a room/device tile toggles that light's local on/off presentation. Normal lights expose brightness and color-temperature controls; RGB-capable lights additionally expose a color control; switch-only lights keep the same card footprint without fake adjustment controls. Brightness is adjusted in 10% steps. Color temperature currently spans 2500 K to 6500 K in 400 K steps. RGB adjustment uses two sliders: hue and saturation, both in 10% steps; saturation is rendered from white to the currently selected hue. Entering an adjustment temporarily replaces the card title/icon area, tapping the same active control exits, switching to another control changes adjustment mode directly, and 30 seconds without adjustment returns the card to its default presentation. These values are currently local UI state only; later Home Assistant/network integration must consume/refresh this state outside the page rather than embedding transport logic into the UI module.

Devices behavior is defined as follows: entering Devices always starts on the Air Conditioner page. Horizontal swipes move one full 640 × 172 page at a time through Air Conditioner, Curtain, Bath Heater and Drying Rack, then continue directly back to Air Conditioner; swiping the opposite direction from Air Conditioner continues directly to Drying Rack. The shared feature shell keeps the existing left-side back interaction above the device pager.

The Curtain page uses three large icon-only controls: Remix Icon `expand-left-right-line` for Open, `pause-line` for Stop and `contract-left-right-line` for Close. No text labels are shown. The page background stays pure black, while two low-opacity blue-gray panels grow inward from the left and right edges as the curtain closes and shrink outward as it opens; there are no fabric folds or realistic curtain artwork. The fully open state still leaves a narrow panel visible at each edge, while the fully closed state keeps a small black center gap so the two sides remain visually separate. Curtain state is represented by opening percentage where `0 = fully closed` and `100 = fully open`. `ui_page_devices_set_curtain_position()` lets external Home Assistant/device logic push the current percentage into the UI without embedding transport logic in the LVGL page. For preview/local interaction, Open and Close animate toward their endpoints and Stop freezes the current visual position.

The Bath Heater page uses a left-side seven-segment temperature display and icon-only controls on the right. While every bath-heater function is stopped, the display shows the detected current temperature supplied through `ui_page_devices_set_bath_current_temperature()` in tenths of a degree. Once any function is active, the display switches to the target temperature; vertical dragging adjusts the target from 16.0 to 31.0 degrees in 0.5-degree steps. The four mode controls are mutually exclusive: Ventilation (`refresh-line`), Blower (`windy-line`), Warm Air (`sun-line`) and Dehumidify (`drop-line`) can never run at the same time. Selecting a different mode stops the previous mode and starts the new one. Ventilation, Blower and Warm Air start at low speed and repeated taps on the currently selected mode toggle between low and high presentation states. Dehumidify is a simple on/off mode. A tall red `stop-circle-line` button sits to the right of the 2 × 2 mode grid and stops every bath-heater function, returning the temperature display to the detected current temperature. No control text labels are shown, and actual device/HA transport remains outside the LVGL page.

The Drying Rack page uses three icon-only controls: Remix Icon `arrow-up-line` and `arrow-down-line` are stacked on the left, while a larger `pause-line` button sits on the right. The center status graphic is deliberately minimal LVGL geometry rather than an ad-hoc icon: a short fixed top rail, two vertical suspension lines and a longer lower rail. Up and Down animate the lower rail and vertical suspension length between the top position and a lower position around two-thirds of the 172 px canvas; Pause freezes the current height. `ui_page_devices_set_drying_rack_position()` lets external device/HA logic set the current vertical travel percentage where `0 = top` and `100 = lowest visual position`.

Timer behavior is defined as follows: the page contains no text labels. It shows only `HH:MM:SS` and circular icon controls. In the setting state, the single large Play control is green. Starting replaces it with two smaller circular controls: an orange Pause control above and a red Stop control below. Pausing freezes the remaining time and changes the upper control to an orange Play/Resume icon. Stop cancels the active countdown and restores the most recently configured duration. When the countdown reaches zero it remains at `00:00:00`; the upper action can immediately replay the last configured duration, while Stop returns to the editable setting state with that duration restored. A zero-duration timer does not start. Button backgrounds are translucent while their icons remain fully colored.

Standby behavior is fixed as follows: after 60 seconds without input, an inactive page returns to the clock. **While a countdown timer is actively running, the automatic 60-second return to the standby clock is suppressed.** Pause, Stop or countdown completion restores the normal idle behavior. There is no standby weather/calendar carousel. A short tap on the clock opens HOME. Upward swiping from standby moves the standby page itself upward with the finger, revealing a stationary HOME page underneath. From HOME, a downward swipe brings the clock in from above to cover HOME. These are intentionally asymmetric inverse transitions. These vertical transitions never return to a previously active function page.

## Alert presentation and physical buttons

Alert UI is authored for the fixed **640 × 172** landscape canvas and must use the same approved Remix Icon asset system as the rest of the product.

- **Alarm ringing:** show a full-screen black overlay above the current page. The centered alarm graphic uses the embedded Remix alarm icon scaled large enough to dominate the 172 px height, with symmetric ringing-wave marks on both sides. The previous page remains alive underneath. Touching anywhere acknowledges the alarm, stops the sound and removes only the overlay, revealing the exact previous page.
- **Timer finished:** automatically bring the real Timer feature page to the foreground so `00:00:00` and the normal Timer controls remain visible. Touching anywhere acknowledges the timer sound but leaves the Timer page open.
- **Alert volume:** timer/alarm playback temporarily uses **80% output volume**. This must not overwrite the user's normal Sound setting. When the alert stops, the codec returns to the saved user volume.
- **Alert timeout:** if nobody acknowledges an alert, audio stops after 10 minutes as a safety limit.
- **Reliability rule:** the audio alert worker is created once during boot, before Wi-Fi starts, and remains resident. Alarm/timer triggers only send commands to this worker; they must not allocate a new FreeRTOS task for every ring. One-shot alarms disable themselves only after playback has actually been accepted. Editing/saving a previously fired one-shot alarm explicitly re-arms it and the new time takes effect immediately.

The enclosure exposes three physical buttons. Their product behavior is:

- **RESET / CHIP_PU:** hardware reset only; application code does not remap it.
- **PWR:** GPIO16 is sampled active-low, matching Waveshare's V2 power example. Holding it for **1.5 seconds** requests power-off. On battery, EXIO6 / `SYS_EN` is driven low to cut the system rail. If USB still powers the MCU, the firmware enters deep sleep after the button is released and uses GPIO16 active-low as the wake source.
- **BOOT / custom key (GPIO0):** after normal boot it acts as Back. While an alarm or timer is ringing it acknowledges/stops the alert first. Otherwise Feature -> HOME, HOME -> standby clock; inside the Alarm editor it closes the editor before leaving the Alarm feature. RESET/download-mode behavior at boot is not changed.

## Cleanup and verification status (2026-10-16)

- Removed obsolete standby carousel event types and unreachable handlers; only the clock standby view exists.
- Restored physical-mounting LCD/touch transform, remapped IMU orientation for Remote icon tilt, and reset stale Remote icon angle on each page rebuild. **Awaiting real-device orientation, touch, and gyro regression tests**.
- Replaced eight runtime-expanded system icon A8 buffers and the generated lock-icon mask with identical read-only alpha data. The expected writable-static-data reduction is 8,768 bytes; verify internal DRAM in the linker map and running firmware before reporting heap gains.
- Deleted uncompiled 44px and 72px temperature-font files; retained the built 84px subset.
- Clock-only WakeNet lifecycle is now wired to committed page state: the dedicated worker serializes start/stop using existing model/AFE teardown. **Not yet firmware-built or soak-tested; heap recovery remains unverified.**
- Wi-Fi STA-start-to-IP and IP-to-first-SNTP timing diagnostics have been added. Optimization decisions must follow measured latency.
- Still pending: full source/resource reachability audit, firmware build, on-device LCD/touch/IMU checks, long-running page-toggle and memory soak tests. Sony TV live-control validation intentionally deferred.

### Remote IMU regression (2026-10-16)

- Hardware confirmed: factory-facing UI rotation and touchscreen corners are correct.
- Real-device report: Remote icons previously rotated for physical counterclockwise 90°, but not clockwise 90°.
- Remote UI now handles `BOARD_ORIENTATION_PORTRAIT_LEFT` for the clockwise 90° icon response; board-wide IMU axis mapping and LCD/touch geometry remain unchanged. **Verify the clockwise response on device after flashing; this is a code fix, not yet a passed hardware retest.**
- WakeNet lifecycle and memory behavior remain separate follow-up work.

### HOME SRAM + clock color changes (2026-10-16)

- Four HOME icons and four HOME text masks previously used 18,160 bytes of writable A8 buffers, initialized by RLE decoding. They now contain **bit-identical, zero-tail-preserving** `static const` A8 data in flash. UI tile geometry, gradients, gestures, labels, and actions are unchanged. Check the firmware linker map and runtime heap before claiming an exact free-heap gain.
- Clock digits and both clock battery icon variants now use cyan `#78DCE8` from smart-terminal. Lock overlay icon style is unchanged.
- Battery inspection (no functional change): `board_power_get_status` reads VBAT via ADC GPIO4; `BOARD_BATTERY_LOW_MV=3500`; low-battery icon only appears below that threshold **when USB Serial/JTAG is not connected**. USB-host detection is used as a surrogate for charging; it **does not prove charging current**. There is no normal-battery symbol. Low-battery glyph flashes every 500 ms, polling battery status every ~1 second. Confirm behavior with USB unplugged; do not confuse USB host presence with charge-current telemetry.
