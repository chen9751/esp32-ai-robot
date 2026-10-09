#!/usr/bin/env python3
"""Verify the immutable Hello Robot V1 training export without third-party packages."""
from pathlib import Path
import hashlib
import sys

ROOT = Path(__file__).resolve().parent.parent
MODEL = ROOT / "models/wakeword/hello_robot_v1/hello_robot_v1.tflite"
EXPECTED_SIZE = 62200
EXPECTED_SHA256 = "ac714ace6399e6a71fce9e26e0dd64cc967b3a1a2438487f4f8701815c50dcda"

def main() -> int:
    if not MODEL.is_file():
        print(f"FAIL: missing model: {MODEL}", file=sys.stderr)
        return 1
    payload = MODEL.read_bytes()
    digest = hashlib.sha256(payload).hexdigest()
    size_ok = len(payload) == EXPECTED_SIZE
    digest_ok = digest == EXPECTED_SHA256
    print(f"Model: {MODEL.relative_to(ROOT)}")
    print(f"Bytes: {len(payload)} (expected {EXPECTED_SIZE})")
    print(f"SHA256: {digest}")
    print(f"Integrity: {'PASS' if size_ok and digest_ok else 'FAIL'}")
    return 0 if size_ok and digest_ok else 1

if __name__ == "__main__":
    raise SystemExit(main())
