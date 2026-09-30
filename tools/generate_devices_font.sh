#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_UI="$ROOT_DIR/components/ui/ui_font_source_han_devices_16.c"
OUT_TEMP="$ROOT_DIR/components/ui/ui_font_source_han_devices_temp_44.c"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

FONT_URL="https://raw.githubusercontent.com/adobe-fonts/source-han-sans/release/OTF/SimplifiedChinese/SourceHanSansSC-Normal.otf"
FONT_FILE="$TMP_DIR/SourceHanSansSC-Normal.otf"
UI_SYMBOLS='开关模式制冷制热风扇干燥摆风睡眠辅热温度计'
TEMP_SYMBOLS='0123456789.°'

echo "Downloading Source Han Sans SC Normal..."
curl -L --fail --retry 3 --silent --show-error "$FONT_URL" -o "$FONT_FILE"

echo "Generating LVGL Devices UI subset..."
npx --yes lv_font_conv \
  --font "$FONT_FILE" \
  --symbols "$UI_SYMBOLS" \
  --size 16 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --lv-font-name ui_font_source_han_devices_16 \
  --lv-include lvgl.h \
  -o "$OUT_UI"

echo "Generating LVGL Devices temperature subset..."
npx --yes lv_font_conv \
  --font "$FONT_FILE" \
  --symbols "$TEMP_SYMBOLS" \
  --size 44 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --lv-font-name ui_font_source_han_devices_temp_44 \
  --lv-include lvgl.h \
  -o "$OUT_TEMP"

echo "Generated: $OUT_UI"
echo "Generated: $OUT_TEMP"
