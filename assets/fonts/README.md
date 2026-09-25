# Font & Icon Assets

This directory defines the project's approved typography and icon sources.

## 1. Source Han Sans CN Normal / 思源黑体 CN Normal

- Role: default Chinese UI typeface.
- Weight: **Normal**.
- Upstream: https://github.com/adobe-fonts/source-han-sans
- Selected upstream font: `SourceHanSansCN-Normal.otf`.
- License: SIL Open Font License 1.1.
- Firmware policy: subset the Chinese/Latin glyphs actually used by the UI before converting to LVGL font data. Do not compile the full CJK font into firmware by default.

## 2. Fusion Pixel Font / 缝合像素字体

- Role: retro clock, LCD/old-display numbers and occasional retro status text.
- Upstream: https://github.com/TakWolf/fusion-pixel-font
- Preferred design family: **12px monospaced** unless a later screen design specifically calls for another variant.
- License: MIT.
- Firmware policy: use only the characters required by clock/status screens and generate a small LVGL subset.

## 3. Remix Icon

- Role: primary UI icon library.
- Upstream: https://github.com/Remix-Design/RemixIcon
- Source font: `fonts/remixicon.ttf`.
- License: Remix Icon License.
- Firmware policy: select only icons actually used by the product and convert those glyphs to LVGL font data at the sizes needed by the UI.

Typical icon categories for this project include remote control, music, light bulb, devices/home, alarm, settings, Wi-Fi, Bluetooth, battery, microphone, speaker, volume and media controls.

## Asset workflow

Keep this distinction throughout development:

```text
Upstream font/icon source
        ↓
assets/fonts source selection
        ↓
glyph/icon subset for a specific UI
        ↓
LVGL generated font (.c)
        ↓
ESP32 firmware
```

The source families are the design baseline. Generated LVGL fonts may use several sizes (for example text 16/20 and icons 24/48/64) without changing the selected visual family.

When adding a new UI page, first check these three families before adding another font or icon dependency.
