# Hello Robot V1: micro_speech frontend + stateful TFLM diagnostic

**Status: synthetic-silence integration test only. Not yet live microphone wake detection.**

This standalone ESP-IDF 5.5.4 app now calls the actual
`esphome/esp-micro-speech-features==1.2.3` C frontend rather than
filling the model input with a constant vector. It configures a 16 kHz,
30 ms window / 10 ms step / 40-channel filterbank with microWakeWord
PCAN, noise reduction and log scaling, uses ESPHome's documented
integer quantization mapping, groups three 40-element frames, and
invokes the verified stateful Hello Robot V1 TFLite Micro model 200
times. The input PCM is **synthetic silence**, not microphone audio.

The documented training session pinned
`kahrendt/microWakeWord@4665173cd35f1cff9a61e06fc427f124766c488e`.
No reference PCM -> feature golden vectors have been compared yet;
the present pipeline parameters derive from the ESPHome microWakeWord
frontend implementation, and need a bitwise/reference comparison against
the exact training library before accuracy claims.

## Mac

```sh
cd ~/Desktop/esp32-ai-robot-wakeword
git pull origin feature/hello-robot-wakeword-model
source ~/.espressif/python_env/idf5.5_py3.9_env/bin/activate
. ~/esp/esp-idf/export.sh
cd test_apps/hello_robot
idf.py build
```

**Flashing this isolated test overwrites the running app and uses a
different partition map**, leaving the original 640 x 172 UI absent.
Do not flash without a rollback/backup. For now obtain a successful
compile result and CI status before testing. No live microphone or wake
event is wired into this application.

The next step is a single-owner ES7210 24 kHz stereo PCM capture,
24-to-16 kHz conversion with channel/aliasing validation, reference
feature equivalence, then speech threshold/false alarm calibration.
The normal firmware/UI and its Hi ESP code are unchanged.
