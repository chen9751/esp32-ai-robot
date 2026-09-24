# UI prototype v1

Target: Waveshare ESP32-S3-Touch-LCD-3.49 V2, ESP-IDF 5.5.4 + LVGL 9.

## Display baseline

Panel native resolution: **172x640**.

Primary landscape UI resolution: **640x172**.

Portrait pages may use **172x640**.

The earlier 320x172 logical prototype size was based on an incorrect resolution assumption and is retired. The panel/board driver remains separate and must follow the Waveshare V2 reference implementation.

## Existing v1 pages

- Home: weather, date/day and large time
- Functions: Remote / Music / Lights / Devices
- System settings: top-down gesture, volume and brightness
- Lights: power, brightness, color temperature and presets
- TV Remote: Apple-TV-inspired compact control surface
- Music: track, artist, progress and transport controls
- Devices: placeholder

## Gestures

- Home horizontal swipe -> Functions
- Top-edge swipe down -> Settings
- Settings swipe up -> previous screen
- Right swipe on feature pages -> Functions

The UI intentionally uses mock values. The current widget coordinates were created for the obsolete 320x172 prototype and will be redesigned rather than stretched. Home Assistant lights, music service, TV commands, RTC/weather and hardware brightness/volume bindings are the next integration layer.
