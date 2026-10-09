# Hello Robot V1 / independent on-device model smoke test

This is **not** the main firmware and **does not** recognize spoken Hello Robot.
It intentionally runs the exact 62,200-byte model, uses a 768 KiB PSRAM
Tensor Arena, and logs TensorFlow Lite Micro allocation/invocation errors,
operator compatibility, score, average/max inference time, and heap deltas.
Input is a constant zero-feature test vector, not microphone PCM.

Run on macOS with ESP-IDF 5.5.4:

```sh
. ~/esp/esp-idf/export.sh
cd ~/Desktop/esp32-ai-robot-wakeword/test_apps/hello_robot
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

**WARNING:** this deliberately replaces the application firmware and
therefore the UI and Hi ESP will not run while this diagnostic is installed.
The root project is unchanged. Restore the main firmware from the repository
root using `idf.py -p PORT flash monitor`. Before flashing the test image,
back up any required NVS or other data, and verify partitions; a fresh test
project defaults to a different partition layout. No wake accuracy can be
concluded from the constant vector. Actual spoken phrase tests require the
training-matched micro_speech feature pipeline, streaming state correctness,
and one owned microphone audio pipeline.
