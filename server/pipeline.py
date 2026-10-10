"""Single file-in/file-out offline voice pipeline. No external devices touched."""
from pathlib import Path
from .agent import run
from .asr import transcribe_file
from .tts import synthesize_wav

def process_voice(input_file, output_wav, *, sample_rate=16000, channels=1):
    recognized = transcribe_file(input_file)
    response = run(recognized["text"])
    answer = response.get("answer") or "Sorry, I cannot answer that."
    output = synthesize_wav(answer, output_wav, sample_rate=sample_rate, channels=channels)
    return {
        "transcript": recognized["text"],
        "language": recognized["language"],
        "language_probability": recognized["language_probability"],
        "answer": answer,
        "intent": response.get("intent"),
        "tool_trace": response.get("tool_trace", []),
        "verified": response.get("verified", False),
        "audio_file": str(Path(output).resolve()),
    }
