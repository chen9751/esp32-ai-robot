"""Local loopback HTTP tests; pipeline and model are mocked."""
import http.client
import io
import wave
import json
import threading
import unittest
from unittest.mock import patch

from server.api import VoiceHandler, VoiceServer

class ApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = VoiceServer(("127.0.0.1", 0), VoiceHandler)
        cls.server.api_token = "test-token-123456789"
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=3)

    def request(self, method, route, body=None, headers=None):
        conn = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=5)
        try:
            conn.request(method, route, body=body, headers=headers or {})
            response = conn.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            conn.close()

    def test_health(self):
        status, _, data = self.request("GET", "/api/health")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(data)["status"], "ok")

    def test_missing_token(self):
        status, _, _ = self.request("POST", "/api/voice", b"abc",
                                   {"Content-Type": "audio/wav"})
        self.assertEqual(status, 401)

    def test_bad_type(self):
        status, _, _ = self.request("POST", "/api/voice", b"abc",
                                   {"Content-Type": "text/plain",
                                    "Authorization": "Bearer test-token-123456789"})
        self.assertEqual(status, 415)

    def test_oversized(self):
        status, _, _ = self.request("POST", "/api/voice", b"x",
                                   {"Content-Type": "audio/wav",
                                    "Authorization": "Bearer test-token-123456789",
                                    "Content-Length": str(2 * 1024 * 1024 + 1)})
        self.assertEqual(status, 413)

    @patch("server.api.process_voice")
    def test_voice_returns_wav(self, mocked):
        def pipeline(source, dest, **kwargs):
            with io.BytesIO() as buffer:
                with wave.open(buffer, "wb") as wav:
                    wav.setnchannels(1)
                    wav.setsampwidth(2)
                    wav.setframerate(16000)
                    wav.writeframes(b"\x00\x00" * 160)
                dest.write_bytes(buffer.getvalue())
            return {"answer": "Hello"}
        mocked.side_effect = pipeline
        status, headers, data = self.request(
            "POST", "/api/voice", b"RIFFtest",
            {"Content-Type": "audio/wav",
             "Authorization": "Bearer test-token-123456789"})
        self.assertEqual(status, 200)
        self.assertEqual(headers["Content-Type"], "audio/wav")
        self.assertEqual(data[:4], b"RIFF")

if __name__ == "__main__":
    unittest.main()
