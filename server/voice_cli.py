"""Offline demo: audio -> Whisper -> Gemma -> Piper WAV; or greeting WAV."""
import argparse
import json
from .pipeline import process_voice
from .tts import synthesize_wav

def main():
    parser = argparse.ArgumentParser(description="Debian ESP32 voice chain")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--input", help="Recorded audio (m4a/wav/etc.)")
    group.add_argument("--hello", action="store_true", help="Generate Amy 'Hello!' greeting")
    parser.add_argument("--output", required=True, help="Output PCM WAV file")
    parser.add_argument("--rate", type=int, default=16000)
    parser.add_argument("--channels", type=int, default=1, choices=(1, 2))
    args = parser.parse_args()
    if args.hello:
        wav = synthesize_wav("Hello!", args.output, args.rate, args.channels)
        print(json.dumps({"text": "Hello!", "audio_file": str(wav),
                          "sample_rate": args.rate, "channels": args.channels}, indent=2))
    else:
        print(json.dumps(process_voice(args.input, args.output,
                                       sample_rate=args.rate, channels=args.channels),
                         ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()
