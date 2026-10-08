# Memory audit and conservative PSRAM optimization — 2026-10-09

## Evidence and limits

- Inspected GitHub `main` and local `/Users/chen/esp32-ai-robot` at
  `0d474c4798cea9241ea7400c77f2c95307fa2975`.
- The saved board boot log identifies firmware `0d474c4`, ESP-IDF 5.5.4,
  8 MiB octal PSRAM at 80 MHz. PSRAM self-test passed.
- `/dev/cu.usbmodem201401` is the ESP32-S3 USB JTAG/serial device.
  `/dev/cu.usbmodem212NTTQ3S8732` is an LG monitor control interface, not another ESP32 port.
- The available monitor log stops partway through audio initialization. It has
  **no complete runtime memory samples**. Do not infer a crash from this truncated
  file, or mistake the UI-stage values for Wi-Fi + HA + WakeNet steady state.
- The execution sandbox refused opening the USB device (`Operation not permitted`).
  No reset, erase, flash, real-device A/B measurement or UI interaction test was performed.

### Observed startup values (bytes)

| Stage | Internal free | Largest internal block | Internal DMA-capable free | PSRAM free |
| --- | ---: | ---: | ---: | ---: |
| Before board | 221763 | 126976 | 214083 | 8386156 |
| After board | 178447 | 102400 | 170767 | 7713488 |
| After UI | 178183 | 102400 | 170503 | 7705192 |

DMA-capable free space overlaps internal free space: never add these columns.
These values are free heaps, not the chip's total installed RAM. Firmware static
sections, reserved regions and heap overhead must be considered separately.

### Existing memory layout

- LVGL custom allocator already explicitly uses PSRAM; the former 64 KiB internal
  `work_mem_int` pool is absent from the linked baseline.
- Three 640 x 172 RGB565 frame buffers: 660480 bytes total, in PSRAM.
- LCD DMA strip: 22016 internal bytes, retained unchanged (64 native rows).
- Voice feed/detect stacks: 6144 bytes each; HA lights/devices: 6144/8192 bytes;
  LVGL: 8192 bytes; physical buttons: 3072 bytes. All sizes remain unchanged.
- Baseline app binary: 2144592 bytes in a 5242880-byte app slot.
  Model image: 291142 bytes in a 5242880-byte model partition. Flash free space
  cannot solve internal runtime RAM pressure; partition layout is unchanged.

The actual local sdkconfig differs from the repository defaults:

| Option | Actual baseline | Repository defaults |
| --- | --- | --- |
| SPIRAM_MALLOC_ALWAYSINTERNAL | 16384 | 4096 |
| SPIRAM_TRY_ALLOCATE_WIFI_LWIP | disabled | enabled |
| ESP_WIFI_IRAM_OPT | enabled | disabled |
| ESP_WIFI_RX_IRAM_OPT | enabled | disabled |

This patch does not change either profile. Existing sdkconfig values take
precedence over defaults; a fresh build can therefore behave differently.

## Changes

1. HA command queue payloads explicitly use PSRAM, with queue control structures
   kept in internal static memory. Counts and item formats remain 32 device
   commands (8832 bytes) and 20 light commands (320 bytes), with identical FIFO,
   command handling, confirmation and polling behavior.
2. Each HA worker owns one reusable PSRAM response buffer. Capacity remains
   3072/2048 bytes of text plus the length field, totaling 5128 bytes on ESP32-S3.
   Synchronous requests already reset response length/content before performing
   HTTP; JSON trees still have their existing allocation/lifetime. This removes
   recurring response-buffer allocation/free from the internal heap.
3. Voice input scratch moves to PSRAM. The checked local driver chain is
   `esp_codec_dev_read -> i2s_channel_read -> memcpy(dest, internal_DMA_buffer, n)`.
   The application destination is not the hardware DMA buffer. I2S descriptors,
   rings, sample rate, stereo-to-mono conversion and all wake logic are unchanged.
4. HA initialization failures now release queue payloads, queue handles,
   response buffers and mutexes, avoiding partial-init leaks on retry.
5. Diagnostics add PSRAM low water/largest block, DMA largest block, heap allocated
   bytes and the voice/HA/button stack low-water marks. The Wi-Fi-stage log now
   occurs before HA init, correcting the previous misleading placement.

No UI source, board/display/touch implementation, GPIO, DMA setting, task stack
size/priority, radio setting, model, partition, network timeout or poll cadence
was changed. Stack scans/logging occur once per minute and have small diagnostic
overhead; they are not a real-time profiling or soak-test substitute.

### Expected effect, not measured savings

- Queue payload migration: 9152 internal bytes removed from dynamic allocations.
- Voice scratch migration: `6 * feed_chunk_samples` bytes; e.g. 3072 bytes if
  the AFE reports 512 samples. Read the new startup log for the actual value.
- HA response buffers: up to 5128 fewer internal bytes during overlapping polls;
  the amount depends on what was active and where malloc previously placed it.
- New persistent PSRAM use: 14280 bytes for queues/responses, plus voice source
  scratch. The mono scratch was already in PSRAM.
- Queue control structures move from heap to BSS. New diagnostics also consume
  a small amount of code/static memory. Do not claim the raw buffer sums as an
  exact net free-heap improvement.

## Validation performed

- All four changed C translation units compiled successfully with the baseline's
  exact ESP32-S3 compiler, include paths, definitions and generated sdkconfig.
- The complete firmware ELF linked successfully with replacement network/voice/main
  archives and unchanged baseline IDF 5.5.4/component libraries and linker scripts.
- Standard clean `idf.py build` was blocked by Component Manager's process-tree
  enumeration under the sandbox. Thus this is a compile/link validation using the
  existing dependency build, **not a successful clean component-resolution build**.
- Linker comparison: D/IRAM static total 218143 -> 218383 bytes (+240), comprising
  BSS +184 and RAM code +56. Non-RAM Flash use +1184 bytes. Map-derived total image
  2144333 -> 2145573 bytes (+1240; binary padding is separate).
- Diff review confirms unchanged UI, board, touch, audio driver and configuration.
- The log parser was exercised on the saved boot log and correctly reported zero
  runtime samples rather than extrapolating them.

Hardware validation remains required before claiming unchanged timing, wake
accuracy, HA response behavior or improved runtime headroom.

## Capture and compare on the Mac

Close any existing serial monitor with Ctrl+]. From the project directory and
ESP-IDF Python environment, capture the current running firmware without reset:

```sh
python tools/capture_memory.py --duration 190 --output memory-before.json
```

Build/flash the optimized source with the existing sdkconfig (do not erase flash
or regenerate sdkconfig). Partition/model selection is unchanged. Run the same
capture after boot/connection settles:

```sh
python tools/capture_memory.py --duration 190 --output memory-after.json
```

The parser also supports `--input saved-monitor.log`. Output contains only
memory/stack/firmware diagnostics, not a full UART transcript.

Compare like-for-like states: Wi-Fi connected, HA configured and polling, WakeNet
active, same BLE on/off state and UI page. Then switch pages, control the existing
HA devices, wake repeatedly, and verify audio read errors remain zero. Exercise
BLE separately if it is part of the intended concurrent workload.

## Budget for the next audio milestone

These are proposed additional allocations, not measurements or guaranteed bounds.
WakeNet/AFE, Wi-Fi, current UI/HA tasks and existing I2S DMA are already present
and must not be counted twice.

| Proposed item | Location | Initial budget |
| --- | --- | ---: |
| 0.5 s pre-roll, 16 kHz mono S16 | PSRAM | 16000 B |
| 1 s uplink ring, same format | PSRAM | 32000 B |
| 2 s downlink ring, 24 kHz mono S16 | PSRAM | 96000 B |
| Framing, staging, resampling scratch | PSRAM | 16–64 KiB |
| Additional transport/playback task stacks | Internal | 12–20 KiB |
| Control objects and incremental networking overhead | Internal | 8–24 KiB allowance |

Thus reserve roughly 0.16–0.25 MiB additional PSRAM (prefer 0.3–0.5 MiB headroom)
and approximately 20–44 KiB internal RAM for an initial PCM half-duplex prototype.
Actual library choices may exceed this allowance; TLS handshakes, Opus and AEC
must each be budgeted and measured separately. Mono downlink stays mono in the
large ring; duplicate into the existing stereo output only in a small staging
buffer. Keeping two seconds of stereo instead would need 192000 bytes.

Use one microphone owner, bounded rings/queues, session IDs, cancellation,
timeouts and independent transport/playback workers. Never block AFE feed/fetch
on a slow server. ASR, LLM and TTS run on the NUC11; don't add local MultiNet just
to transmit speech. The current wake callback/transport/playback service still
needs integration; this patch does not implement the voice conversation chain.

Before trimming more RAM, measure the complete workload. If internal memory is
still tight, next candidates are measured stack right-sizing and a separate A/B
test of Wi-Fi PSRAM allocation. Keep the existing LCD DMA size initially. Fix
WakeNet stop/restart resource ownership before implementing repeated AFE rebuilds.
Acceptance should include repeated conversations, disconnect/reconnect and
long-running tests with bounded queues and no downward free-heap trend.

References:
- https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/api-reference/peripherals/i2s.html
- https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/api-reference/system/heap_debug.html
- https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/benchmark/README.html
