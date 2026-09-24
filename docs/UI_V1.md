# UI prototype v1

Target: Waveshare ESP32-S3-Touch-LCD-3.49 V2, ESP-IDF 5.5.4 + LVGL 9.

Logical layout is 320x172 landscape for the first prototype. The panel/board driver remains separate and must follow the Waveshare V2 reference implementation.

Pages:
- Home: weather, date/day and large time
- Functions: Remote / Music / Lights / Devices
- System settings: top-down gesture, volume and brightness
- Lights: power, brightness, color temperature and presets
- TV Remote: Apple-TV-inspired compact control surface
- Music: track, artist, progress and transport controls
- Devices: placeholder

Gestures:
- Home horizontal swipe -> Functions
- Top-edge swipe down -> Settings
- Settings swipe up -> previous screen
- Right swipe on feature pages -> Functions

The UI intentionally uses mock values. Home Assistant lights, music service, TV commands, RTC/weather and hardware brightness/volume bindings are the next integration layer.
