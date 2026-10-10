"""Minimal LM Studio tool-calling loop; tools are simulated until explicitly integrated."""
import json
import os
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError

BASE_URL = os.getenv("LMS_BASE_URL", "http://127.0.0.1:1234/v1").rstrip("/")
MODEL = os.getenv("LMS_MODEL", "esp32-chat")
SYSTEM = ("You are a friendly voice assistant. Understand Chinese and English. "
          "Always answer in English, briefly and naturally. No Markdown. "
          "Never claim an external action succeeded unless its tool result confirms it.")

TOOLS = [
    {"type": "function", "function": {
        "name": "get_weather", "description": "Look up current weather in a city (SIMULATED only).",
        "parameters": {"type": "object", "properties": {"city": {"type": "string"}},
                       "required": ["city"], "additionalProperties": False}}},
    {"type": "function", "function": {
        "name": "set_light", "description": "Set a Home Assistant room light (SIMULATED only).",
        "parameters": {"type": "object", "properties": {
            "room": {"type": "string"}, "brightness_pct": {"type": "integer", "minimum": 0, "maximum": 100}},
            "required": ["room", "brightness_pct"], "additionalProperties": False}}},
    {"type": "function", "function": {
        "name": "search_story", "description": "Find an English story for later authorized playback (SIMULATED only).",
        "parameters": {"type": "object", "properties": {"query": {"type": "string"}},
                       "required": ["query"], "additionalProperties": False}}},
]

def execute_tool(name, args):
    """Deliberately NO network writes, device control or media downloads."""
    if not isinstance(args, dict):
        return {"ok": False, "error": "arguments must be an object"}
    if name == "get_weather":
        city = args.get("city")
        if not isinstance(city, str) or not city.strip() or len(city) > 100:
            return {"ok": False, "error": "invalid city"}
        return {"ok": True, "simulated": True, "city": city, "weather": "mock: sunny", "temperature_c": 22}
    if name == "set_light":
        room, level = args.get("room"), args.get("brightness_pct")
        if not isinstance(room, str) or not room.strip() or len(room) > 100 or type(level) is not int or not 0 <= level <= 100:
            return {"ok": False, "error": "invalid room or brightness"}
        return {"ok": True, "simulated": True, "room": room, "brightness_pct": level}
    if name == "search_story":
        query = args.get("query")
        if not isinstance(query, str) or not query.strip() or len(query) > 200:
            return {"ok": False, "error": "invalid query"}
        return {"ok": True, "simulated": True, "results": [
            {"title": "The Tale of Peter Rabbit (public-domain text candidate)",
             "query": query, "playable": False}]}
    return {"ok": False, "error": "tool not allowed"}

def completion(messages, *, use_tools=True):
    payload = {"model": MODEL, "messages": messages, "temperature": 0.2,
               "max_tokens": 160, "stream": False}
    if use_tools:
        payload.update(tools=TOOLS, tool_choice="auto")
    data = json.dumps(payload, ensure_ascii=False).encode()
    req = Request(BASE_URL + "/chat/completions", data=data,
                  headers={"Content-Type": "application/json"}, method="POST")
    try:
        with urlopen(req, timeout=90) as response:
            result = json.load(response)
    except (HTTPError, URLError, TimeoutError) as exc:
        raise RuntimeError(f"LMS connection failed: {exc}") from exc
    choices = result.get("choices") or []
    if not choices:
        raise RuntimeError("LMS returned no completion choices")
    return choices[0]["message"]

def run(text, max_tool_rounds=2):
    if not text.strip():
        raise ValueError("Empty message")
    messages = [{"role": "system", "content": SYSTEM}, {"role": "user", "content": text}]
    trace = []
    for turn in range(max_tool_rounds + 1):
        message = completion(messages, use_tools=(turn < max_tool_rounds))
        calls = message.get("tool_calls") or []
        if not calls:
            return {"answer": message.get("content") or "", "tool_trace": trace}
        if turn >= max_tool_rounds:
            break
        messages.append({"role": "assistant", "content": message.get("content"), "tool_calls": calls})
        for call in calls[:3]:
            fn = call.get("function") or {}
            name = fn.get("name", "")
            try:
                args = json.loads(fn.get("arguments", "{}"))
            except (json.JSONDecodeError, TypeError):
                args = None
            result = execute_tool(name, args)
            trace.append({"tool": name, "arguments": args, "result": result})
            messages.append({"role": "tool", "tool_call_id": call.get("id", ""),
                             "name": name, "content": json.dumps(result, ensure_ascii=False)})
    return {"answer": "Sorry, I couldn't complete that request.", "tool_trace": trace}
