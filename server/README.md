# Debian LMS backend — plain-text JSON intent prototype

This experimental server code runs on Debian and **does not modify ESP32 firmware, LVGL, or the fixed 640 × 172 UI**.

## Runtime

- LM Studio / llmster, model `esp32-chat` (Gemma 3 4B QAT, 8192 context)
- OpenAI-compatible API `http://127.0.0.1:1234/v1`
- Python standard library only
- Fixed default location: **Wuhua District, Kunming, Yunnan, China**, `Asia/Shanghai` (from `server/config.json`). Explicit locations may override it.

## Why this prototype does not use native tool calling

On this environment, native `tool_calls` remained empty with both `auto` and `required`. LMS `json_schema` failed in llama.cpp grammar sampler initialization with runtimes 2.41.0 and 2.55.0. The supported `text` response mode was observed generating a valid JSON object inside a Markdown fence. The backend now uses plain-text JSON with strict parsing and validation.

## Run tests on Debian

```bash
cd ~/esp32-ai-robot
git pull --ff-only
python3 -m unittest discover -s server/tests -v
time python3 -m server.cli '今天的天气怎么样？'
time python3 -m server.cli '把客厅灯的亮度调到30%'
time python3 -m server.cli '找一个彼得兔的英文故事'
time python3 -m server.cli '你好，请用英语介绍自己'
```

The `intent` field shows model-selected action/parameters, while `tool_trace` records simulated dispatch. If the model emits non-JSON or invalid arguments the backend fails closed. Chat has a conservative external-action keyword guard as an interim safeguard.

**All external tools are simulations.** A weather request does not fetch conditions, a light request does not contact Home Assistant, and a story request does not search or play audio. `verified: false` is always returned for tool requests, even on a successful simulation. No fictitious weather readings are supplied. Rooms currently allowed are `living_room`, `bedroom`, `study`, `kitchen`; these are placeholders, not configured HA entities.

Do not rely on one successful example: collect a Chinese/English regression suite and real-model success rates. The next implementation steps are reliable intent routing, tool allowlist mapping to actual HA entities, genuine weather lookup, and verified English speech output. Never commit tokens, model weights, recordings, logs, or private configuration.


## Offline voice pipeline (Whisper + Gemma + Piper Amy)

Requires Debian local installation:
- `~/ai-voice/venv` with `faster-whisper`, `numpy`, and `piper-tts`
- Multilingual Whisper `base` cached at `~/ai-voice/models`
- `~/ai-voice/models/piper/en_US-amy-medium.onnx` plus its `.onnx.json`
- System `ffmpeg` and LMS model `esp32-chat`.

First, activate voice venv and update branch:

```bash
cd ~/esp32-ai-robot
git pull --ff-only
source ~/ai-voice/venv/bin/activate
python -m unittest discover -s server/tests -v
python -m server.voice_cli --input ~/ai-voice/recordings/test-zh.m4a --output ~/ai-voice/recordings/reply.wav
ffprobe -v error -show_entries stream=codec_name,sample_rate,channels,bits_per_sample -of default=noprint_wrappers=1 ~/ai-voice/recordings/reply.wav
```

Create a short greeting in the selected Amy voice:

```bash
python -m server.voice_cli --hello --output ~/ai-voice/recordings/hello.wav --rate 16000 --channels 1
```

**TF-card playback compatibility:** The repository/history did not establish the exact existing `hello` filename/path/format for the current firmware. Earlier I2S initialization logs noted 16 kHz / 16-bit / stereo, which is NOT sufficient evidence about the stored file encoding. Do not overwrite the TF file until you inspect the actual existing file and playback code. If the existing file is a WAV and its format differs, regenerate the greeting with the matching rate/channels or explicitly convert with FFmpeg (`-c:a pcm_s16le`). For raw PCM, `hello.wav` with a RIFF header may not play; confirm file extension and decoder before copying. Preserve the old TF file as a backup.

This first-stage script processes **existing audio files only**, not a live ESP32 upload service. ASR uses FFmpeg -> 16k mono float32 arrays to avoid a known PyAV `metadata_errors` API incompatibility. Input capped at 20MiB and 30s of audio. Generated playback WAV defaults to 16k mono, signed 16-bit PCM; this is a test default, not a proven requirement of the board.

Caution: external actions are STILL mocked. The generated voice speaks the safe simulation message, not actual weather/HA success. No firmware or UI files have been changed.
