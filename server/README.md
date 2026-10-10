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
