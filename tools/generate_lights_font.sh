#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT_DIR/components/ui/ui_font_source_han_lights_18.c"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

FONT_URL="https://raw.githubusercontent.com/adobe-fonts/source-han-sans/release/OTF/SimplifiedChinese/SourceHanSansSC-Normal.otf"
FONT_FILE="$TMP_DIR/SourceHanSansSC-Normal.otf"
SYMBOLS='客厅灯书房卧室床头小彩光带浴阳台播放源电视音箱'

echo "Downloading Source Han Sans SC Normal..."
curl -L --fail --retry 3 --silent --show-error "$FONT_URL" -o "$FONT_FILE"

echo "Generating LVGL Lights subset..."
npx --yes lv_font_conv \
  --font "$FONT_FILE" \
  --symbols "$SYMBOLS" \
  --size 18 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --lv-font-name ui_font_source_han_lights_18 \
  --lv-include lvgl.h \
  -o "$OUT"

echo "Generated: $OUT"
