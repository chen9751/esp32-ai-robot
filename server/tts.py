"""Piper Amy synthesis plus explicit PCM WAV conversion for ESP32 playback."""
import os
import subprocess
import tempfile
from pathlib import Path

MODEL = Path(os.getenv("PIPER_VOICE", str(Path.home() / "ai-voice/models/piper/en_US-amy-medium.onnx")))

def synthesize_wav(text, output_path, sample_rate=16000, channels=1):
    """Synthesize English text to PCM S16LE WAV.

    sample_rate and channels MUST match the ESP32 firmware's actual file decoder.
    Do not infer file format from codec/I2S initialization alone.
    """
    if not isinstance(text, str) or not text.strip() or len(text) > 1500:
        raise ValueError("Invalid TTS input")
    if sample_rate not in (16000, 22050, 24000, 32000, 44100, 48000):
        raise ValueError("Unsupported sample rate")
    if channels not in (1, 2):
        raise ValueError("Unsupported channel count")
    if not MODEL.is_file() or not MODEL.with_suffix(".onnx.json").is_file():
        raise FileNotFoundError("Piper Amy model and JSON must be installed: " + str(MODEL))
    out = Path(output_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="piper-") as temp_dir:
        raw = Path(temp_dir) / "piper.wav"
        subprocess.run(
            ["python", "-m", "piper", "--model", str(MODEL),
             "--output_file", str(raw)],
            input=text.strip() + "\n", text=True,
            capture_output=True, check=True, timeout=90,
        )
        # Convert audio generated at Piper model rate into a device-oriented file.
        subprocess.run(
            ["ffmpeg", "-nostdin", "-y", "-v", "error", "-i", str(raw),
             "-ac", str(channels), "-ar", str(sample_rate),
             "-c:a", "pcm_s16le", str(out)],
            capture_output=True, check=True, timeout=45,
        )
    if not out.is_file() or out.stat().st_size < 44:
        raise RuntimeError("TTS output missing or empty")
    return out
