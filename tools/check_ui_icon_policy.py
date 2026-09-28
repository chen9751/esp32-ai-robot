#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
LIGHTS = ROOT / "components/ui/ui_page_lights.c"
ICONS = ROOT / "components/ui/ui_lights_icons.c"
ESP_CMAKE = ROOT / "components/ui/CMakeLists.txt"
WEB_CMAKE = ROOT / "web_preview/CMakeLists.txt"

errors = []
lights = LIGHTS.read_text(encoding="utf-8")
icons = ICONS.read_text(encoding="utf-8")

# Semantic glyphs must not drift back to hand-drawn LVGL geometry.
# Match legacy identifiers as complete C identifiers so approved names such as
# UI_LIGHTS_ICON_SOFA / UI_LIGHTS_ICON_BED are not rejected as substrings.
legacy_identifiers = (
    "create_room_icon",
    "create_small_icon",
    "ICON_SOFA",
    "ICON_PC",
    "ICON_BED",
    "create_beam",
)
for token in legacy_identifiers:
    if re.search(rf"(?<![A-Za-z0-9_]){re.escape(token)}(?![A-Za-z0-9_])", lights):
        errors.append(f"Lights page contains forbidden legacy semantic-icon token: {token}")

if "beam[" in lights:
    errors.append("Lights page contains forbidden legacy semantic-icon token: beam[")

for token in ('#include "ui_lights_icons.h"', "UI_LIGHTS_ICON_SOFA", "UI_LIGHTS_ICON_BRIGHTNESS"):
    if token not in lights:
        errors.append(f"Lights page is missing approved icon reference: {token}")

# Keep provenance explicit so replacements can be audited against the approved library.
for source in (
    "Others/sofa-line.svg",
    "Device/computer-line.svg",
    "Map/hotel-bed-line.svg",
    "Others/lightbulb-line.svg",
    "Device/tv-line.svg",
    "Design/drop-line.svg",
    "Business/window-line.svg",
    "Weather/sun-line.svg",
    "Weather/temp-hot-line.svg",
    "Design/palette-line.svg",
):
    if source not in icons:
        errors.append(f"Remix Icon source provenance missing: {source}")

for path in (ESP_CMAKE, WEB_CMAKE):
    if "ui_lights_icons.c" not in path.read_text(encoding="utf-8"):
        errors.append(f"{path.relative_to(ROOT)} does not build ui_lights_icons.c")

if errors:
    print("UI icon policy check FAILED:")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("UI icon policy check passed: Lights semantic icons are sourced from Remix Icon.")
