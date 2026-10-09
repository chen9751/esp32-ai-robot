# Hello Robot V1 — wake-word model

This directory holds the **candidate** custom wake-word model for the ESP32-S3 client. It is a **model asset only**; adding this file does not enable wake-word detection.

| Field | Value |
| --- | --- |
| Wake phrase | `Hello Robot` |
| Format | Streaming, quantized TensorFlow Lite (`.tflite`) |
| Model family | microWakeWord MixedNet (64-filter configuration) |
| Training source | [microWakeWord](https://github.com/kahrendt/microWakeWord), commit `4665173cd35f1cff9a61e06fc427f124766c488e` |
| Model file | `hello_robot_v1.tflite` |
| File size | 62,200 bytes (60.74 KiB) |
| SHA-256 | `ac714ace6399e6a71fce9e26e0dd64cc967b3a1a2438487f4f8701815c50dcda` |
| Input | `int8 [1, 3, 40]`, scale `0.10196078568696976`, zero-point `-128` |
| Output | `uint8 [1, 1]`, scale `0.00390625`, zero-point `0` |

The model was exported from the best weights after training on synthetic speech and audio augmentation. Quantization calibration used positive training features only; negative/noise calibration and realistic false-trigger tests remain outstanding. Reported validation metrics are from synthetic data, **not** field performance.

## Integration status

**Not yet tested on the Waveshare ESP32-S3-Touch-LCD-3.49 V2.** The ESP-IDF firmware does not automatically load this model. Integration requires a compatible TensorFlow Lite Micro runtime, streaming state-variable support, the matching 16 kHz / 40-feature audio preprocessing pipeline, and measured Tensor Arena, memory, and inference latency. Preserve the existing Hi ESP behavior and the **640 × 172** LVGL UI until isolated hardware tests pass.

This folder intentionally excludes training datasets, checkpoints and intermediate SavedModels. Keep those in the separate training backup.

## Integrity check

```sh
shasum -a 256 models/wakeword/hello_robot_v1/hello_robot_v1.tflite
```
