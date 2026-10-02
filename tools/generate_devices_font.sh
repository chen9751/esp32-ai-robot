#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_UI="$ROOT_DIR/components/ui/ui_font_source_han_devices_16.c"
OUT_TEMP="$ROOT_DIR/components/ui/ui_font_source_han_devices_temp_84.c"
OUT_REMIX="$ROOT_DIR/components/ui/ui_font_remix_devices_28.c"
OUT_REMIX_LARGE="$ROOT_DIR/components/ui/ui_font_remix_devices_56.c"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

SOURCE_HAN_URL="https://raw.githubusercontent.com/adobe-fonts/source-han-sans/release/OTF/SimplifiedChinese/SourceHanSansSC-Normal.otf"
REMIX_URL="https://raw.githubusercontent.com/Remix-Design/RemixIcon/master/fonts/remixicon.ttf"
SOURCE_HAN_FILE="$TMP_DIR/SourceHanSansSC-Normal.otf"
REMIX_FILE="$TMP_DIR/remixicon.ttf"

# Right-side labels stay textual; all left-side controls use Remix Icon glyphs.
UI_SYMBOLS='睡眠干燥辅热ECOAUTO1234567'
TEMP_SYMBOLS='0123456789.°'
# Remix Icon v4.x code points:
# power=f126, snowflake=f512, sun=f1bf, windy=f2ca,
# drop=ec6a, swing=ea62, temp-cold=f1f2.
REMIX_SYMBOLS=$'\uF126\uF512\uF1BF\uF2CA\uEC6A\uEA62\uF1F2'
# Large device controls additionally use:
# expand-left-right=f323, pause=efd8, contract-left-right=f2ff.
REMIX_LARGE_SYMBOLS=$'\uF1F2\uF323\uEFD8\uF2FF'

echo "Downloading Source Han Sans SC Normal..."
curl -L --fail --retry 3 --silent --show-error "$SOURCE_HAN_URL" -o "$SOURCE_HAN_FILE"
echo "Downloading Remix Icon font..."
curl -L --fail --retry 3 --silent --show-error "$REMIX_URL" -o "$REMIX_FILE"

echo "Generating LVGL Devices UI subset..."
npx --yes lv_font_conv \
  --font "$SOURCE_HAN_FILE" \
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
  --font "$SOURCE_HAN_FILE" \
  --symbols "$TEMP_SYMBOLS" \
  --size 84 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --lv-font-name ui_font_source_han_devices_temp_84 \
  --lv-include lvgl.h \
  -o "$OUT_TEMP"

echo "Generating LVGL Devices Remix Icon subset..."
npx --yes lv_font_conv \
  --font "$REMIX_FILE" \
  --symbols "$REMIX_SYMBOLS" \
  --size 28 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --lv-font-name ui_font_remix_devices_28 \
  --lv-include lvgl.h \
  -o "$OUT_REMIX"

echo "Generating large LVGL Devices control icons..."
npx --yes lv_font_conv \
  --font "$REMIX_FILE" \
  --symbols "$REMIX_LARGE_SYMBOLS" \
  --size 56 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --lv-font-name ui_font_remix_devices_56 \
  --lv-include lvgl.h \
  -o "$OUT_REMIX_LARGE"

echo "Generated: $OUT_UI"
echo "Generated: $OUT_TEMP"
echo "Generated: $OUT_REMIX"
echo "Generated: $OUT_REMIX_LARGE"
