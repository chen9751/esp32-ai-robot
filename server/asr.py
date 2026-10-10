"""Offline faster-whisper transcription. FFmpeg decoding avoids PyAV API mismatch."""
import os
import subprocess
from functools import lru_cache
from pathlib import Path

MAX_AUDIO_BYTES = 20 * 1024 * 1024
MAX_AUDIO_SECONDS = 30

@lru_cache(maxsize=1)
def get_model():
    from faster_whisper import WhisperModel
    return WhisperModel(
        os.getenv("ASR_MODEL", "base"),
        device="cpu", compute_type="int8",
        cpu_threads=int(os.getenv("ASR_CPU_THREADS", "4")),
        num_workers=1,
        download_root=str(Path.home() / "ai-voice/models"),
    )

def transcribe_file(path):
    import numpy as np
    path = Path(path)
    if not path.is_file() or not 0 < path.stat().st_size <= MAX_AUDIO_BYTES:
        raise ValueError("Audio file missing, empty, or too large")
    # FFmpeg stdout is bounded indirectly by a 30-second input clip + timeout.
    command = [
        "ffmpeg", "-nostdin", "-v", "error", "-i", str(path),
        "-t", str(MAX_AUDIO_SECONDS), "-f", "f32le", "-ac", "1", "-ar", "16000", "pipe:1"
    ]
    proc = subprocess.run(command, capture_output=True, check=True, timeout=45)
    if not proc.stdout:
        raise ValueError("Decoded audio is empty")
    audio = np.frombuffer(proc.stdout, dtype=np.float32)
    segments, info = get_model().transcribe(
        audio, beam_size=3, vad_filter=True,
        initial_prompt="昆明，五华区，客厅，卧室，Home Assistant。"
    )
    text = "".join(segment.text for segment in segments).strip()
    if not text:
        raise ValueError("No recognizable speech")
    return {"text": text, "language": info.language,
            "language_probability": round(float(info.language_probability), 3)}
