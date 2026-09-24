# UI assets

This directory is the stable resource namespace for the 640x172 UI.

Planned runtime mapping:
- Web Preview: `assets/...`
- ESP32 SD card: `/sdcard/assets/...`
- Flash fallback: built-in emergency resources

Home weather backgrounds use these stable IDs:
- weather/sunny_day
- weather/cloudy_day
- weather/rain_day
- weather/clear_night
- weather/cloudy_night
- weather/rain_night

The UI must request resource IDs rather than hard-code storage paths. This keeps the same UI source usable by Web Preview and the physical ESP32-S3.
