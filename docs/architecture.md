# Architecture

## Product boundary

This repository is the ESP32-S3 client for the AI/home-control device. Heavy AI work remains on the Debian/NUC11 host. The ESP32 owns the display, touch, audio I/O, local interaction state, Wi-Fi/BLE connectivity and Home Assistant commands.

The current product UI is always **640 x 172 landscape**. The Waveshare ESP32-S3-Touch-LCD-3.49 V2 panel is physically wired as **172 x 640**; rotation is a board/runtime concern and must not leak into page layout code.

## Layering

```text
+--------------------------------------------------+
| UI pages (LVGL, logical 640 x 172)               |
| clock / home / remote / music / lights / devices|
| alarm / settings                                 |
+-------------------------+------------------------+
                          |
                   UI intent/state bridge
                          |
+-------------------------v------------------------+
| Application services                            |
| Home Assistant / Wi-Fi / BLE / AI / audio       |
+-------------------------+------------------------+
                          |
+-------------------------v------------------------+
| Board runtime                                    |
| LCD / touch / backlight / power / I2C / QSPI    |
| Waveshare ESP32-S3-Touch-LCD-3.49 V2            |
+--------------------------------------------------+
```

UI code must not manipulate GPIO, I2C, SPI, BLE, Wi-Fi or Home Assistant directly. Service and board code must not create or mutate LVGL objects without entering the common LVGL lock.

## LVGL runtime

Firmware and Web Preview are pinned to LVGL 9.2.2 so one UI source tree is exercised against one API version.

On hardware, `esp_lvgl_port` owns the LVGL timer/task and common mutex. Any LVGL call made outside an LVGL event/timer callback must be wrapped by:

```c
board_display_lock(0);
/* LVGL calls */
board_display_unlock();
```

Callbacks invoked by LVGL should remain short and non-blocking. Network, HA, BLE and AI operations should be queued to their own tasks rather than performed synchronously in a touch callback.

## Display and touch transform

The board layer registers the AXS15231B LCD at its native 172 x 640 resolution and enables software rotation. LVGL applies `LV_DISPLAY_ROTATION_90`, exposing a 640 x 172 logical display to the UI.

The AXS15231B touch controller reports coordinates in its raw 640 x 172 orientation. The board touch configuration first maps this to native LCD coordinates with mirror-X + swap-XY. LVGL 9.2.2 then applies the display rotation to pointer coordinates as part of input processing.

All panel pins, buses and transform flags live in `components/board/include/board_config.h`. If the first physical integration test reveals a mirrored axis, fix the board configuration only; do not compensate inside individual UI pages.

## Startup order

1. Configure backlight PWM at 0%.
2. Initialize system I2C and TCA9554 I/O expander.
3. Keep BL_EN off, assert SYS_EN, reset the LCD through TCA9554.
4. Initialize AXS15231B QSPI LCD.
5. Initialize touch I2C and AXS15231B touch controller.
6. Start `esp_lvgl_port`, register display and touch, and set 90-degree software rotation.
7. Lock LVGL and build the UI with `ui_init()`.
8. Unlock LVGL and enable the backlight.

This avoids exposing an uninitialized/white LCD while the UI tree is being built.

## Settings and service state

The settings page is presentation, not ownership. Brightness, volume, Wi-Fi, BLE and AI state are ultimately owned by board/service modules. `ui_page_settings.h` provides the boundary for injecting state and reporting user intent. The next service-integration work should connect these callbacks to real modules rather than adding hardware calls to the page itself.

## Home Assistant and AI

Lights and devices should primarily be represented as Home Assistant entities. Touch UI and AI voice commands should both resolve to the same application command layer, for example:

```text
Touch -> LIGHT_SET_BRIGHTNESS -> HA service
Voice -> AI intent -> LIGHT_SET_BRIGHTNESS -> HA service
```

This prevents separate control logic for touch and voice.

AI speech recognition/LLM/TTS is therefore mostly independent of page layout. UI only needs transient listening/thinking/speaking state and resulting command/status feedback.

## Next hardware integrations

After LCD/touch runtime is stable on the physical board, add modules in this order:

1. Real brightness callback and persistent settings.
2. Wi-Fi provisioning + NVS + reconnect state.
3. NTP/RTC synchronization.
4. Home Assistant transport and entity state subscriptions.
5. Battery/charging detection and settings battery percentage.
6. Audio codec/microphone/speaker service.
7. AI STT/LLM/TTS protocol.
8. OTA update, watchdog/recovery and diagnostics.

Keep these as services; avoid growing the UI layer into a hardware/network layer.
