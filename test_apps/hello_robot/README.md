# Hello Robot V1 — experimental live microphone diagnostic

This independent test is **not the production firmware**. It uses the real
Waveshare ESP32-S3-Touch-LCD-3.49 **V2** `board_init()` and the same
`audio_service_init()`/ES7210 capture implementation as the existing robot,
without starting Hi ESP/WakeNet (no competing I2S microphone readers).

Pipeline: 24 kHz stereo S16 -> provisional left-channel 16 kHz mono adapter
-> esp-micro-speech-features frontend (40 features, 30ms/10ms)
-> INT8 [1,3,40] -> stateful Hello Robot V1 TFLM -> serial score.

The 16kHz adapter is the existing prototype 3-to-2 mapping and is **not**
a production-quality filtered resampler. Input channel choice and sample
accuracy must be confirmed in hardware. The frontend uses ESPHome
microWakeWord settings; bitwise identity against the training Colab
reference has not yet been tested. Scores are diagnostic only.
Candidate threshold 180/256 and three consecutive scores are
**uncalibrated**, not proof that the wake word is accurately detected.

## Build (Mac, ESP-IDF v5.5.4)

```sh
cd ~/Desktop/esp32-ai-robot-wakeword
git pull origin feature/hello-robot-wakeword-model
source ~/.espressif/python_env/idf5.5_py3.9_env/bin/activate
. ~/esp/esp-idf/export.sh
cd test_apps/hello_robot
idf.py fullclean
idf.py build
```

Wait for `Project build complete` before considering flashing. The app now
reuses board/LVGL/codec components and its custom factory partition is 5 MiB.
It is intentionally isolated from the production app and does not build the
robot navigation interface. The LCD may be blank because no UI is created.
A flash replaces the existing partition table/app. Back up any required data
and verify the rollback plan first. Restore the production firmware from the
repository root using `idf.py -p PORT flash` (not `app-flash`).

## Hardware test

```sh
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Speak Hello Robot multiple times, keep silent for a period, and speak
unrelated phrases. Record `LIVE_MIC` and
`HELLO ROBOT CANDIDATE DETECTED` logs plus failures and latency.
The 2000 invocation loop should cover roughly 60 seconds of audio
plus initialization; restart to repeat the experiment. If board or codec
setup fails, the program stops rather than substituting synthetic data.

This is a **first hardware experiment** pending clean ESP-IDF build and
on-device validation. It is not a verified shipped voice feature.
