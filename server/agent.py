"""LM Studio plain-text JSON intent parser and safe simulated tool dispatcher."""
import json
import os
import re
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

BASE_URL = os.getenv("LMS_BASE_URL", "http://127.0.0.1:1234/v1").rstrip("/")
MODEL = os.getenv("LMS_MODEL", "esp32-chat")
CONFIG = json.loads(Path(__file__).with_name("config.json").read_text(encoding="utf-8"))
DEFAULT_CITY = CONFIG["location"]["city"] + ", " + CONFIG["location"]["district"]

INTENT_SYSTEM = (
    "You classify bilingual Chinese/English requests for a smart voice assistant. "
    "Output exactly ONE JSON object, preferably without Markdown fences. No other text. "
    "Use one of these schemas, with EXACT keys:\n"
    '{"action":"chat","answer":"brief English response"}\n'
    '{"action":"get_weather","city":"Kunming, Wuhua"}\n'
    '{"action":"set_light","room":"living_room","brightness_pct":30}\n'
    '{"action":"search_story","query":"Peter Rabbit"}\n'
    '{"action":"none"}\n'
    "Weather (including forecast/rain questions) => get_weather. If no place is stated, "
    "use Kunming, Wuhua, Yunnan, China. If a different place is given, use that place. "
    "Changing a light brightness => set_light; extract the actual user-requested percentage. "
    "Do not invent brightness or room. If required values are missing use action none. "
    "For living room use living_room; other rooms may use their literal name. "
    "Finding a story => search_story, extract title/search phrase. "
    "General conversation => chat and answer in English. "
    "Never claim an external action completed. Do not include executable commands."
)

FENCE = re.compile(r"\A```(?:json)?[ \t]*\r?\n(.*?)\r?\n```\Z", re.I | re.S)
ACTIONS = frozenset(("chat", "get_weather", "set_light", "search_story", "none"))
ROOMS = frozenset(("living_room", "bedroom", "study", "kitchen"))
EXTERNAL_WORDS = (
    "天气", "气温", "下雨", "温度", "预报", "weather", "forecast", "rain",
    "灯", "light", "亮度", "开灯", "关灯", "故事", "story", "搜索", "查找",
    "找一个", "找个", "search", "find", "播放", "play", "音乐", "music",
    "闹钟", "alarm", "提醒", "remind", "设备", "device", "打开", "关闭",
)

def parse_json_response(content):
    if not isinstance(content, str) or len(content) > 4096:
        raise ValueError("Invalid model response")
    value = content.strip()
    match = FENCE.fullmatch(value)
    if match:
        value = match.group(1).strip()
    obj = json.loads(value)
    if type(obj) is not dict:
        raise ValueError("Expected a JSON object")
    return obj

def validate_intent(obj):
    """Return a normalized intent. Never trust model output directly."""
    if type(obj) is not dict or obj.get("action") not in ACTIONS:
        raise ValueError("Unknown action")
    action = obj["action"]
    fields = {
        "chat": {"action", "answer"},
        "none": {"action"},
        "get_weather": {"action", "city"},
        "set_light": {"action", "room", "brightness_pct"},
        "search_story": {"action", "query"},
    }[action]
    if set(obj) - fields:
        raise ValueError("Unexpected intent fields")
    if action == "chat":
        answer = obj.get("answer")
        if not isinstance(answer, str) or not answer.strip() or len(answer) > 1000:
            raise ValueError("Invalid chat answer")
        return {"action": action, "answer": answer.strip()}
    if action == "none":
        return {"action": action}
    if action == "get_weather":
        city = obj.get("city", DEFAULT_CITY)
        if not isinstance(city, str) or not city.strip() or len(city) > 100:
            raise ValueError("Invalid city")
        return {"action": action, "city": city.strip()}
    if action == "set_light":
        room, level = obj.get("room"), obj.get("brightness_pct")
        if not isinstance(room, str) or room not in ROOMS:
            raise ValueError("Room not allowlisted or not configured")
        if type(level) is not int or not 0 <= level <= 100:
            raise ValueError("Invalid brightness")
        return {"action": action, "room": room, "brightness_pct": level}
    query = obj.get("query")
    if not isinstance(query, str) or not query.strip() or len(query) > 200:
        raise ValueError("Invalid story query")
    return {"action": action, "query": query.strip()}

def execute_tool(name, args):
    """Simulation ONLY. No network calls, external actions or media downloads."""
    if not isinstance(args, dict):
        return {"ok": False, "error": "Invalid arguments"}
    if name == "get_weather":
        try:
            intent = validate_intent({"action": name, **args})
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        return {"ok": True, "simulated": True, "city": intent["city"],
                "note": "No live weather data was queried"}
    if name == "set_light":
        try:
            intent = validate_intent({"action": name, **args})
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        return {"ok": True, "simulated": True, "room": intent["room"],
                "brightness_pct": intent["brightness_pct"]}
    if name == "search_story":
        try:
            intent = validate_intent({"action": name, **args})
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        return {"ok": True, "simulated": True, "query": intent["query"],
                "results": [], "note": "No search was performed"}
    return {"ok": False, "error": "Tool not allowed"}

def completion(messages):
    payload = {"model": MODEL, "messages": messages, "temperature": 0,
               "max_tokens": 240, "stream": False}
    req = Request(BASE_URL + "/chat/completions",
                  data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
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

def run(text):
    if not isinstance(text, str) or not text.strip() or len(text) > 2000:
        raise ValueError("Invalid user message")
    trace = []
    try:
        message = completion([{"role": "system", "content": INTENT_SYSTEM},
                              {"role": "user", "content": text}])
        intent = validate_intent(parse_json_response(message.get("content")))
    except (ValueError, TypeError, KeyError, json.JSONDecodeError) as exc:
        return {"answer": "Sorry, I couldn't safely understand that request.",
                "intent": None, "tool_trace": trace, "verified": False,
                "error": type(exc).__name__}
    action = intent["action"]
    if action == "chat":
        # A chat classification must not bypass external-action safety.
        if any(word in text.lower() for word in EXTERNAL_WORDS):
            return {"answer": "I couldn't verify or perform that request.",
                    "intent": intent, "tool_trace": trace, "verified": False}
        return {"answer": intent["answer"], "intent": intent, "tool_trace": trace,
                "verified": True}
    if action == "none":
        return {"answer": "I need more details before I can do that.",
                "intent": intent, "tool_trace": trace, "verified": False}
    args = {key: value for key, value in intent.items() if key != "action"}
    result = execute_tool(action, args)
    trace.append({"tool": action, "arguments": args, "result": result})
    if not result.get("ok"):
        answer = "I couldn't complete that request."
    elif result.get("simulated"):
        answer = "This was a simulation only. No real-world action or live lookup was performed."
    else:
        # Keep fail-closed until verified integrations are implemented.
        answer = "The tool result was not independently verified."
    return {"answer": answer, "intent": intent, "tool_trace": trace, "verified": False}
