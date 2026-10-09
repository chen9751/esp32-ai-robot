#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/tf卡/audio/hello.wav"
mkdir -p "$(dirname "$OUT")"
if ! command -v ffmpeg >/dev/null; then
  echo 'ffmpeg is required to encode WAV as 24kHz stereo PCM16' >&2
  exit 1
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
if command -v espeak >/dev/null; then
  espeak -v en-us+f3 -s 155 -p 65 -a 160 -w "$TMP/hello-source.wav" 'Hello!'
elif command -v say >/dev/null; then
  say -v Samantha -o "$TMP/hello-source.aiff" 'Hello!'
else
  echo 'espeak (Linux) or say (macOS) required' >&2
  exit 1
fi
SOURCE="$TMP/hello-source.wav"
[[ -f "$SOURCE" ]] || SOURCE="$TMP/hello-source.aiff"
ffmpeg -nostdin -loglevel error -y -i "$SOURCE" -ar 24000 -ac 2 -c:a pcm_s16le "$OUT"
echo "Created $OUT"
