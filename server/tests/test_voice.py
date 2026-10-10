"""Offline tests: no model downloads or external calls."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from server.pipeline import process_voice
from server.tts import synthesize_wav

class VoiceTests(unittest.TestCase):
    @patch("server.pipeline.synthesize_wav")
    @patch("server.pipeline.run")
    @patch("server.pipeline.transcribe_file")
    def test_voice_pipeline(self, asr, agent, tts):
        asr.return_value = {"text": "你好", "language": "zh", "language_probability": 0.99}
        agent.return_value = {"answer": "Hello!", "intent": {"action": "chat"},
                              "tool_trace": [], "verified": True}
        tts.return_value = Path("/tmp/reply.wav")
        result = process_voice("/tmp/input.m4a", "/tmp/reply.wav")
        self.assertEqual(result["transcript"], "你好")
        self.assertEqual(result["answer"], "Hello!")
        tts.assert_called_once_with("Hello!", "/tmp/reply.wav",
                                    sample_rate=16000, channels=1)

    def test_tts_invalid_parameters(self):
        for kwargs in ({"sample_rate": 8000}, {"channels": 3}):
            with self.assertRaises(ValueError):
                synthesize_wav("Hello!", "/tmp/nonexistent.wav", **kwargs)

    def test_tts_empty_rejected(self):
        with self.assertRaises(ValueError):
            synthesize_wav("", "/tmp/nonexistent.wav")

    @patch("server.asr.get_model")
    def test_missing_audio(self, model):
        from server.asr import transcribe_file
        with self.assertRaises(ValueError):
            transcribe_file("/definitely/not/existing.wav")
        model.assert_not_called()

if __name__ == "__main__":
    unittest.main()
