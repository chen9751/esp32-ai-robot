# Hello Robot V1 — real-microphone integration contract (2026-10-10)

## Proven on ESP32-S3

The exact V1 model loaded in ESP-IDF 5.5.4 on hardware at commit `d854bd4`.
`AllocateTensors()` passed; 200 invocations passed. Average inference 4292 us,
maximum 5167 us on constant features. Allocator reported 23,268 bytes of
model tensor arena use, with 768 KiB reserved and an additional 128 KiB
resource variable arena. This is NOT a real wake test.

## Distinct pipeline stages

1. Board: existing `audio_service_capture_read` is the **single** mic reader,
   delivering ES7210 interleaved 24 kHz stereo signed 16-bit PCM. Reuse it,
   do not open a second I2S RX on the same I2S controller.
2. Channel selection & 24k->16k mapping: `main/pcm_stream.h` contains
   the exact mapping currently used by WakeNet's AFE feeder, but **left-slot
   selection must still be checked on actual board**. This helper is
   independently host-tested. It is not a complete anti-alias filter;
   production quality may require a better resampler with filter state.
3. Training-matched feature frontend: upstream microWakeWord documents
   16 kHz mono / 30 ms window / 10 ms stride / 40-channel micro_speech
   frontend (noise suppression and PCAN). The repo currently lacks the
   **specific export-time** frontend scaling/normalization settings and
   a golden PCM->features test vector from the Colab run. Do not substitute
   a generic FFT/MFCC/Mel pipeline and claim accuracy.
4. Each completed 3 x 40 INT8 feature group goes to the **stateful**
   model without reinitializing its resource variables, once per nominal
   30 ms. Validate bit-equivalence against the original Python inference
   on at least 5-10 seconds of the SAME feature stream.
5. Score logic (threshold + consecutive/window aggregation + cooldown):
   threshold must be calibrated against real positive speech and ambient
   negatives, not the constant zero-feature inference demo.

## Acceptance before a microphone wake demo

- Recover training notebook/export config or generate a reference audio PCM
  and feature-vector fixture from the same Colab frontend.
- Verify exact quantization conversion to the model's INT8 input (scale
  0.10196078568696976, zero-point -128).
- Confirm channel/slot, amplitude, sample cadence and dropped frame counts
  on device. Avoid any concurrent Hi ESP mic reader.
- Confirm detection with the actual phrase, and run background negatives.
- Only then bridge a wake event into the untouched 640x172 UI.

Keep the working production branch and partition table unchanged.
