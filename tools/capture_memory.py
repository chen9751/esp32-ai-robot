#!/usr/bin/env python3
"""Capture memory-only UART diagnostics without sending data or resetting the ESP32.

Run with the ESP-IDF Python environment (pyserial is already installed).
Close idf.py monitor first. --input parses an existing log without opening USB.
"""

import argparse
import json
import re
import time
from pathlib import Path

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
MEMORY = re.compile(r"MEM ([^:]+): (.*)")
FIELD = re.compile(r"([A-Za-z_]+)=(\d+)")


def collect_line(raw, records):
    line = ANSI.sub("", raw).strip()
    match = MEMORY.search(line)
    if match:
        fields = {key: int(value) for key, value in FIELD.findall(match[2])}
        stage = match[1]
        # Ignore truncated startup/runtime records rather than report partial data.
        if stage != "detail" and not all(
            key in fields for key in ("internal", "largest", "DMA", "PSRAM")
        ):
            return
        records.append({"stage": stage, "bytes": fields, "log": line})
        print(line, flush=True)
    elif "STACK " in line or "PCM scratch in PSRAM:" in line or "PSRAM buffers:" in line:
        records.append({"stage": "diagnostic", "log": line})
        print(line, flush=True)
    elif "App version:" in line:
        records.append({"stage": "firmware", "log": line})
        print(line, flush=True)


def summarize(records):
    runtime = [r["bytes"] for r in records if r["stage"] == "runtime"]
    result = {
        "runtime_samples": len(runtime),
        "units": "bytes (KiB = bytes / 1024)",
        "note": "DMA overlaps internal RAM; do not add them. No audio-load/soak claim.",
        "records": records,
    }
    if runtime:
        result["runtime_latest"] = runtime[-1]
        result["runtime_observed_ranges"] = {
            key: {"min": min(r[key] for r in runtime if key in r),
                  "max": max(r[key] for r in runtime if key in r)}
            for key in runtime[-1]
        }
    else:
        result["note"] += " No complete runtime sample captured; startup is not steady state."
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem201401")
    parser.add_argument("--duration", type=float, default=190,
                        help="seconds; 190 normally captures at least 3 one-minute samples")
    parser.add_argument("--input", type=Path, help="parse a saved log instead of USB")
    parser.add_argument("--output", type=Path, help="write JSON report to this path")
    args = parser.parse_args()
    if args.duration <= 0:
        parser.error("--duration must be positive")
    records = []
    try:
        if args.input:
            for line in args.input.read_text(errors="replace").splitlines():
                collect_line(line, records)
        else:
            import serial
            port = serial.Serial(port=None, baudrate=115200, timeout=1, exclusive=True)
            port.dtr = False
            port.rts = False
            port.port = args.port
            port.open()
            try:
                print("Listening only; no serial writes or reset sequence. Ctrl+C finishes early.",
                      flush=True)
                deadline = time.monotonic() + args.duration
                pending = b""
                while time.monotonic() < deadline:
                    pending += port.read(min(max(port.in_waiting, 1), 4096))
                    while b"\n" in pending:
                        line, pending = pending.split(b"\n", 1)
                        collect_line(line.decode("utf-8", "replace"), records)
                    # Bound malformed/non-newline input; do not grow without limit.
                    if len(pending) > 16384:
                        pending = b""
            finally:
                port.close()
    except KeyboardInterrupt:
        print("\nCapture stopped.")
    result = summarize(records)
    print(json.dumps({k: v for k, v in result.items() if k != "records"},
                     ensure_ascii=False, indent=2))
    if args.output:
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
        print("Saved:", args.output)


if __name__ == "__main__":
    main()
