# Hello Robot V1 — isolated inference bring-up

Status: **not yet implemented or tested on target hardware**. This is a test contract, not a claim that the existing firmware can execute the model.

## Baseline / non-regression

- Hardware: Waveshare ESP32-S3-Touch-LCD-3.49 **V2**, 16 MB flash, 8 MB PSRAM.
- Toolchain: ESP-IDF v5.5.4; preserve existing partition table.
- Keep the production `main` firmware, 640 × 172 LVGL display, touch interactions, Wi-Fi, Bluetooth, I2S/audio, and Hi ESP/WakeNet9 completely unchanged until benchmark acceptance.
- Do **not** attach another microphone reader to the running AFE.
- Record baseline serial output and free internal 8-bit heap, largest internal block, DMA-capable heap, PSRAM free, and minimum heaps before enabling any experiment.

## Preconditions for TFLite Micro

1. Verify the 62,200-byte model with `python3 tools/verify_hello_robot_model.py`.
2. Inspect the FlatBuffer operator codes (including versions), tensor shapes, variable tensors and quantization parameters. Document **every** operator and matching `MicroMutableOpResolver` registration. Schema validity and checksum alone do not establish inference compatibility.
3. Use a version-pinned Espressif `esp-tflite-micro` component compatible with ESP-IDF 5.5.4. Do not add it to the production firmware until the isolated build is proven.
4. Implement correct persistent streaming-state initialization and reset, then compare multiple sequential invocations against reference inference using identical feature inputs.
5. Generate feature frames using the **same** 16 kHz frontend, 40-feature pipeline, normalization and 3-frame stride as training. An all-zero input only establishes a smoke test; it does not validate detection.
6. Test arena allocation with progressively sized buffers and report exact arena size, location (internal RAM / PSRAM), free heaps before and after, and maximum single `Invoke()` duration and p50/p95 duration for >1000 invocations.
7. Measure with Wi-Fi connected, Hi ESP active, and the normal UI animation running before considering simultaneous runtime integration. Report any watchdog, audio dropout, touch lag or UI frame-stall.
8. Include negative speech, background audio and natural Hello Robot recordings; measure false accepts/hour and actual wake recall. Synthetic validation data is insufficient.

## Safe test progression

- **Phase A — integrity:** checksum + FlatBuffer metadata and operator inventory (no firmware change).
- **Phase B — standalone inference:** separate test executable / build flag with mocked prerecorded features, no microphone ownership, no UI or wake-event routing; build and record reproducible logs.
- **Phase C — coexistence:** only after Phase B passes, benchmark production configuration with the existing WakeNet untouched. If resources are insufficient, stop instead of silently disabling WakeNet.
- **Phase D — audio integration:** only after the above, tee an appropriately synchronized copy of PCM from the single audio owner and implement the training-matched feature extractor. Explicitly separate wake score from wake action.

## Safety before flashing

Back up the currently working build artifacts and record git commit, `sdkconfig`, partition layout, flash size, USB port and monitor output. Do not `erase-flash`, `fullclean` a needed local build, change the partition table, or merge a draft PR just to test a model. First flash only a separately built, reviewed test image with an explicit rollback path. `idf.py -p /dev/cu.usbmodemXXXX flash monitor` must run **on the connected Mac**, not in a remote GitHub session.

## Required acceptance report

- Commit ID / IDF version / model checksum; full operator list and resolved versions.
- `AllocateTensors()` result, arena bytes, internal / DMA / PSRAM deltas.
- Repeated `Invoke()` pass/fail, matched expected scores and reproducible inputs.
- p50, p95, max invocation time, streaming cadence versus real-time budget.
- Display/touch/audio/network regressions and false-wake measurements.
- Hardware flash and serial monitor logs.

No performance or hardware success should be recorded without real results.
