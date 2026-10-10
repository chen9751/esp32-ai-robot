# Debian AI backend — first iteration

This is a **non-destructive experimental** LMS tool-calling proof of concept. It does **not** change ESP-IDF, LVGL, screen orientation/resolution (**640 × 172**) or existing firmware behavior.

## Environment

- Debian 13 VM on NUC11, approximately 10 GB allocated memory
- LM Studio / llmster already running on Debian itself
- Gemma 3 4B QAT Q4_0, CPU, context length 8192, identifier `esp32-chat`
- OpenAI-compatible server on `http://127.0.0.1:1234/v1`
- Python standard library only (no pip installs needed yet)

## Running

From repository root:

```bash
lms ps
lms server status
python3 -m unittest discover -s server/tests -v
python3 -m server.cli '请查询昆明天气'
python3 -m server.cli '请把客厅灯调到30%'
python3 -m server.cli '找一个Peter Rabbit英文故事'
python3 -m server.cli '你好，介绍一下自己'
```

If LMS is not running, start its daemon/server and load the model explicitly:

```bash
lms load gemma-3-4b-it-qat --gpu off --context-length 8192 --parallel 1 --identifier esp32-chat
lms server start --port 1234
```

Optional environment overrides: `LMS_BASE_URL`, `LMS_MODEL`.

## Safety and present limitations

**All three tools are mocks only.** No Home Assistant devices, weather sites, YouTube/media services, or timers are contacted. Weather and story results are fictitious placeholders, not live facts. The returned `tool_trace` marks all actions as `simulated: true`; never announce them as real operations.

Model tool use is not guaranteed: LM Studio can expose OpenAI-compatible tool calling, but Gemma 3 4B may not consistently produce correctly structured tool calls. If model replies without `tool_calls`, the result will contain only text; that is a test outcome, not proof a tool ran.

The prototype has a hard limit of two tool rounds and three tool calls per round. Unknown tools are refused; arguments are validated. No subprocess execution, filesystem writes, secrets, or unrestricted networking are available through these tools.

This first step **does not yet implement** ASR, TTS, ESP32 HTTP/WebSocket transport, persistent chat history, streaming, HA, actual weather or licensed media playback. Those features should be added as separately tested modules after the tool-calling baseline works. Do not commit tokens, API keys, *.gguf, logs, WAV data or personal settings.

## Next validation

Run the four CLI examples, inspect `tool_trace` for intended tool selection/arguments, and record response time and success rates before moving to real integrations.
