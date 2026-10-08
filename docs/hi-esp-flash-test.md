# Hi ESP — first hardware flash test

Branch: `feature/hello-robot-wakenet`. This is an **uncompiled hardware-test candidate**, not a verified release.

- Target: Waveshare ESP32-S3-Touch-LCD-3.49 **V2**.
- UI remains 640 × 172 landscape.
- Temporary offline phrase: **Hi ESP**, ESP-SR WakeNet9 model `wn9_hiesp`.
- ESP-SR dependency pinned to 2.4.5.
- `partitions.csv` changed: original SPIFFS `storage` reduced to 0xE0000 (896 KiB) and new `model` partition occupies 0xB00000–0xFFFFFF (5 MiB). **Existing SPIFFS content in the removed storage range will be overwritten on flashing. Back up anything important.**
- Microphone API reads ES7210 signed PCM at 24 kHz stereo. Voice currently converts 24k -> 16k mono with low-cost interpolation and uses a single microphone (first channel). This is a first-link diagnostic, not final-quality audio resampling.
- WakeNet owns the one audio reader; the old microphone diagnostic is not started concurrently.

## On macOS

```sh
cd ~/Desktop
git clone -b feature/hello-robot-wakenet --single-branch https://github.com/chen9751/esp32-ai-robot.git esp32-ai-robot-voice
cd esp32-ai-robot-voice
. ~/esp/esp-idf/export.sh
idf.py --version
idf.py reconfigure
idf.py menuconfig
```

In `menuconfig`, verify under **ESP Speech Recognition** that the **WakeNet9 / Hi ESP (wn9_hiesp)** model is enabled; other WakeNet models and MultiNet should be disabled. Verify **Partition Table -> Custom partition CSV**, filename `partitions.csv`. Save and exit.

```sh
idf.py build
ls /dev/cu.usbmodem*
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

**Use `flash`, not `app-flash`**, because ESP-SR's CMake flash hook must also write the model image to the `model` partition. Do not call `erase-flash` unless you intentionally want to wipe unrelated data.

Expected success logs (indicative only):
```text
ES7210 microphone RX ready
WakeNet started; expected phrase: Hi ESP
WAKE WORD DETECTED: Hi ESP
```

Test with the English phrase “Hi ESP” several times at roughly 0.5–1 m. UI must remain responsive. A successful compile alone does not prove the real ES7210 microphone slot/channel order. If wake detection does not trigger, inspect input levels/channel data and model partition before changing thresholds.

### Build/debug workflow
- If component dependency download fails: capture the exact `idf.py reconfigure` output.
- If build fails: capture the **first** compiler error and preceding context, not only final `ninja failed`.
- If audio init fails: capture logs beginning at `before I2S`, `ES8311`, `ES7210`.
- If AFE init fails: verify `CONFIG_SR_WN_WN9_HIESP=y` inside generated `sdkconfig` and that `model` is included during `flash`.
- If audio frames are valid but undetected: confirm the first channel is the physical microphone and the 24->16k conversion is producing non-silent PCM. Do not assume a successful codec open proves usable samples.

No changes have been made to the main branch.
