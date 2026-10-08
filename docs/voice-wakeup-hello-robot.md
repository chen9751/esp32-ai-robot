# Hello Robot — offline wake word integration

Status: **ES7210 RX integrated in source; compilation and real-device verification pending. WakeNet remains unimplemented.**

## Requirements

- Hardware: Waveshare ESP32-S3-Touch-LCD-3.49 **V2** (16 MB flash, 8 MB PSRAM).
- UI: preserve the current landscape **640 × 172** LVGL 9 screens and the macOS/web preview.
- Wake phrase: **“Hello Robot”** (English), offline and always listening when enabled.
- Engine: Espressif **ESP-SR / WakeNet**, with a model trained for that exact phrase.
- First milestone: log a wake-detection event over serial. Do not require Debian, Wi-Fi, or UI changes.

## Important blocker: model availability

WakeNet is a **trained wake-word detector**, not a text phrase recognizer. Defining `HELLO_ROBOT` in code does **not** create a “Hello Robot” model. Do not label an existing `wn9_hiesp` or `wn9_hilexin` model as “Hello Robot”: those models recognize different phrases.

Before claiming the target phrase works, acquire/train and license a WakeNet model for **Hello Robot**. Espressif documents a wake-word customization service and TTS-trained model submissions:
- https://github.com/espressif/esp-sr
- https://github.com/espressif/esp-sr/issues/88
- https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/wake_word_engine/ESP_Wake_Words_Customization.html

A stock WakeNet model (e.g. **Hi ESP**) may be used **only as a temporary pipeline diagnostic**. Its actual spoken phrase must be stated accurately in serial logs; passing this test is not passing the Hello Robot requirement.

## Repository findings (main branch at start)

- `main/main.c` initializes `board`, the existing 640×172 UI, `audio_service`, network, and BLE. Voice must not operate on LVGL objects outside the LVGL mutex/task.
- `components/audio/audio_service.c` originally initialized **ES8311 playback only**. The branch now adds **ES7210 RX**, at the same **24 kHz**, 16-bit, stereo setting, with a consumer-driven PCM read API and a diagnostic worker. No hardware verification has yet been performed.
- I2S TDM signals are currently defined in `audio_service.c`: MCLK GPIO7, BCLK GPIO15, WS GPIO46, DOUT GPIO45. Do not invent the ES7210 DIN pin or microphone slot order; get both from Waveshare V2 sources/schematic.
- Existing `partitions.csv` reserves two 5 MB app slots and a large `storage` SPIFFS partition. Verify the ESP-SR model-partition mechanism and final firmware sizes before editing this layout.
- ESP-IDF baseline is **5.5.4**. Pin a compatible ESP-SR release; do not blindly take latest.

## Implementation checkpoints

1. **Baseline**: record free/largest internal heap, PSRAM, firmware/app size, LVGL performance; backup current working firmware.
2. **RX driver**: reuse Waveshare V2 ES7210 initialization, share I2C and TDM clock settings correctly with the current ES8311 TX path, and provide a single 16 kHz signed PCM microphone stream to the engine. Current 24 kHz TX does not satisfy a 16 kHz WakeNet input. Agree on a clock-rate strategy explicitly; avoid creating two independent drivers for the same I2S controller.
3. **Capture diagnostic**: emit measured sample rate, channel/slot arrangement, overrun counts, task stack watermark and audio RMS. Verify real microphone samples.
4. **ESP-SR**: pin release compatible with IDF 5.5.4, provision model storage, feed the exact chunk/channel layout requested by the chosen AFE config, and log the **actual** detected model ID.
5. **Hello Robot model**: install verified corresponding model, repeat 100+ wake trials, record false alarms and latency.
6. **UI event bridge**: use an event queue; only then add 640×172 overlay animations, without importing speech libraries into UI modules.

## Audio ownership and performance constraints

The audio service must own RX/TX and codec hardware lifecycle; a separate WakeNet worker must not reinitialize ES8311/ES7210 or the same I2S port. Prefer I2S DMA-capable **internal RAM** for DMA descriptors/buffers and PSRAM for larger non-DMA buffers. Never block DMA capture on LVGL rendering or Wi-Fi. Keep raw audio processing out of the LVGL task.

Acceptance criteria for **completed** Hello Robot milestone: offline wake with this exact phrase, no fake model aliases, repeatable detection after many wake cycles, no stream overruns, and no UI regression. Until then, status stays **not implemented**.

## Microphone integration (current branch)

Waveshare **V2** source of truth: `ESP-IDF/08_Audio_Test/components/codec_board/board_cfg.txt` specifies **MCLK GPIO7, BCLK GPIO15, WS GPIO46, DIN GPIO6, DOUT GPIO45** for board `S3_LCD_3_49`; input codec **ES7210**. This is confirmed against the official 08_Audio_Test `user_audio_bsp.c` recording path.

- `audio_service_init()` creates paired I2S0 TDM RX and TX channels, retains the working ES8311 output, initializes ES7210 via the existing I2C0 bus, and opens stereo signed-16-bit capture at 24 kHz.
- `audio_service_capture_read(pcm, bytes)` returns interleaved raw captured PCM. Call from exactly **one reader task**.
- `app_main()` starts a temporary always-on microphone-level diagnostic every 2 seconds; listen/speak near the board and verify the reported `avg_abs` and `peak` change substantially. This is **not** wake-word detection.
- The 24-kHz setting is preserved to avoid inadvertently changing speaker pitch or playback behavior. **WakeNet expects 16-kHz mono/AFE frames**; this is a separate later change, and requires slot/channel verification.
- Build and flash on the Mac first: `. ~/esp/esp-idf/export.sh`, `idf.py build`, `idf.py -p /dev/cu.usbmodemXXXX flash monitor`. A successful build does not prove the two real microphone channels have the correct mapping or signal gain.

If ES7210 codec setup fails, audio_service logs a warning; do not assume the diagnostic output will appear. If the board has insufficient internal DMA memory, capture initialization may fail and the I2S RX/TX buffer plan will need adjustment. Do not merge until successful on-device test with UI + Wi-Fi + BLE together.
