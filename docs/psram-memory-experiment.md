# PSRAM-first optimization experiment (ESP32-S3 3.49 V2)

## Goal and baseline

Keep all existing 640×172 GUI/touch, ES7210 input, ES8311 output, TF-card configuration, Wi-Fi/HA settings, WakeNet Hi ESP, and optional BLE Settings control. No screen or feature was deleted.

Measured baseline with Wi-Fi + AFE active and BLE off:
- after board: internal=114547, largest=40960
- after audio: internal=110119, largest=31744
- after Wi-Fi: internal=68931, largest=29696
- after AFE: internal=28715, largest=9216, PSRAM=7318124
- on-demand BLE init fails with ESP_ERR_NO_MEM when largest block is ~7168.

## Changes on this branch

1. ESP-IDF config defaults: SPIRAM_USE_MALLOC=y, SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y, SPIRAM_MALLOC_ALWAYSINTERNAL=4096, DMA/internal reserved pool remains 32768; Wi-Fi IRAM fast-paths disabled to reduce internal memory pressure. These are deliberate latency-vs-memory trades and need hardware testing.
2. WakeNet AFE: trial `AFE_MODE_HIGH_PERF` instead of `AFE_MODE_LOW_COST`. Official benchmark shows some configurations improve internal RAM at the cost of PSRAM, but the benchmark's MR input geometry differs from this project's M (one-mic) config, so a saving is NOT guaranteed.
3. Resampling output scratch (`mono`) explicitly allocated in PSRAM; I2S capture input remains in internal RAM.
4. Added AFE before/after memory logs. Existing boot-stage heap log remains.

## Important: existing sdkconfig OVERRIDES sdkconfig.defaults

If building in the same local checkout, `idf.py reconfigure` does not automatically replace values already stored in `sdkconfig`. Inspect with:

```sh
grep -E '^(CONFIG_SPIRAM_USE_MALLOC|CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP|CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL|CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL|CONFIG_ESP_WIFI_IRAM_OPT|CONFIG_ESP_WIFI_RX_IRAM_OPT)=' sdkconfig
```

To apply new defaults safely, back up local sdkconfig, then use `idf.py menuconfig` to explicitly set these six options (PSRAM and Wi-Fi menus), or generate a new sdkconfig in a fresh build directory with defaults applied. Do **not** delete `sdkconfig` unless you know which board-specific settings are retained in it. Never erase flash/TF card as part of this experiment.

## Build/test

```sh
cd ~/Desktop/esp32-ai-robot-voice
git pull origin feature/hello-robot-wakenet
idf.py menuconfig
idf.py build
idf.py -p /dev/cu.usbmodem201401 flash monitor
```

Do not flash if compilation fails. Copy the new `MEM ...`, `AFE before create`, `AFE after create`, Wi-Fi IP, Hi ESP detections, and BLE-on errors. Note: Wi-Fi/PSRAM allocation policy, HIGH_PERF, and scratch relocation have not been proven stable yet. AFE must still recognize Hi ESP; disable HIGH_PERF and compare if it cannot.

## Rollback

Known previous tested version: e7879bb (Wi-Fi + Hi ESP working, BLE ON fails). Avoid overwriting the current working branch with a detached test checkout. Use a second clone at that commit to build a known-good baseline, or cherry-pick/revert experimental changes as appropriate.

## Next phase (not part of this commit)

Single audio-owner pipeline for wake and network streaming, fixed-size PSRAM audio ring buffer, stream transport, NUC11 test echo, long-run memory high-water-mark validation. This is intentionally not included until the memory experiment is compiled and tested.
