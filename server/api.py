"""Small authenticated LAN HTTP bridge: WAV/M4A upload -> Whisper -> Gemma -> Amy WAV.

Not Internet-facing. No ESP32 firmware changes are required to test this server.
"""
import argparse
import hmac
import json
import os
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

from .pipeline import process_voice

MAX_UPLOAD = 2 * 1024 * 1024
PROCESS_LOCK = threading.Lock()
CONTENT_TYPES = {"audio/wav": ".wav", "audio/x-wav": ".wav",
                 "audio/wave": ".wav", "audio/mp4": ".m4a",
                 "application/octet-stream": ".wav"}


class VoiceHandler(BaseHTTPRequestHandler):
    server_version = "RobotVoice/0.1"

    def respond_json(self, status, obj):
        payload = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def authenticated(self):
        expected = self.server.api_token
        provided = self.headers.get("Authorization", "")
        return hmac.compare_digest(provided, "Bearer " + expected)

    def do_GET(self):
        if urlsplit(self.path).path == "/api/health":
            self.respond_json(200, {"status": "ok", "service": "robot-voice",
                                    "voice": "en_US-amy-medium",
                                    "audio_output": "wav/pcm_s16le/16000/mono",
                                    "authenticated_upload": True})
        else:
            self.respond_json(404, {"error": "not found"})

    def do_POST(self):
        if urlsplit(self.path).path != "/api/voice":
            return self.respond_json(404, {"error": "not found"})
        params = urlsplit(self.path).query
        # The existing ESP32 codec accepts 24kHz/stereo; default remains 16kHz/mono.
        if params == "rate=24000&channels=2":
            rate, channels = 24000, 2
        elif not params:
            rate, channels = 16000, 1
        else:
            return self.respond_json(400, {"error": "unsupported audio output parameters"})
        if not self.authenticated():
            return self.respond_json(401, {"error": "unauthorized"})
        content_type = self.headers.get("Content-Type", "").split(";", 1)[0].lower()
        if content_type not in CONTENT_TYPES:
            return self.respond_json(415, {"error": "expected audio/wav or audio/mp4"})
        size_raw = self.headers.get("Content-Length")
        try:
            size = int(size_raw)
        except (TypeError, ValueError):
            size = 0
        if not 0 < size <= MAX_UPLOAD:
            return self.respond_json(413, {"error": "invalid audio size (max 2MiB)"})
        # Only one inference at once: protects the 10GB VM.
        if not PROCESS_LOCK.acquire(blocking=False):
            return self.respond_json(503, {"error": "busy; retry later"})
        try:
            with tempfile.TemporaryDirectory(prefix="robot-audio-") as directory:
                source = Path(directory) / ("input" + CONTENT_TYPES[content_type])
                target = Path(directory) / "reply.wav"
                remaining = size
                with source.open("wb") as dest:
                    while remaining:
                        block = self.rfile.read(min(65536, remaining))
                        if not block:
                            return self.respond_json(400, {"error": "incomplete upload"})
                        dest.write(block)
                        remaining -= len(block)
                result = process_voice(source, target, sample_rate=rate, channels=channels)
                audio = target.read_bytes()
                if not audio.startswith(b"RIFF") or audio[8:12] != b"WAVE":
                    raise ValueError("Generated audio is not a WAV")
                self.send_response(200)
                self.send_header("Content-Type", "audio/wav")
                self.send_header("Content-Length", str(len(audio)))
                self.send_header("Cache-Control", "no-store")
                # Transcript/intent are not exposed in headers; avoids encoding and privacy issues.
                self.end_headers()
                self.wfile.write(audio)
        except (ValueError, OSError, RuntimeError) as exc:
            self.log_error("voice processing failed: %s: %s", type(exc).__name__, str(exc)[:200])
            self.respond_json(422, {"error": "voice processing failed", "type": type(exc).__name__})
        except Exception as exc:
            self.log_error("unexpected voice error: %s", type(exc).__name__)
            self.respond_json(500, {"error": "internal error"})
        finally:
            PROCESS_LOCK.release()


class VoiceServer(ThreadingHTTPServer):
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    token = os.getenv("ROBOT_API_TOKEN")
    if not token or len(token) < 16:
        parser.error("Set ROBOT_API_TOKEN (at least 16 chars); do not commit it")
    server = VoiceServer((args.host, args.port), VoiceHandler)
    server.api_token = token
    print(f"Voice API on http://{args.host}:{args.port} (POST /api/voice)", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
