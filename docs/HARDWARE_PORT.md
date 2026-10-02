# Waveshare ESP32-S3-Touch-LCD-3.49 V2 hardware port

The hardware layer is based on the verified Waveshare **V2** ESP-IDF examples. Do not substitute V1 or similarly named board pin maps.

## Display

- Controller: AXS15231B
- Interface: QSPI, SPI3_HOST
- Native panel resolution: 172 x 640
- Project logical UI resolution: 640 x 172
- LCD CS: GPIO9
- LCD PCLK: GPIO10
- LCD DATA0..3: GPIO11..14
- LCD TE: GPIO21
- LCD reset: TCA9554 bit 5 (not GPIO21)
- Backlight PWM: GPIO42
- Backlight power-enable: TCA9554 bit 1

The board runtime registers the display at 172 x 640 and uses LVGL 9.2.2 software rotation (`LV_DISPLAY_ROTATION_90`) to expose 640 x 172 to all page code.

## I2C and I/O expander

System I2C bus:
- port 0
- SCL GPIO48
- SDA GPIO47

Touch I2C bus:
- port 1
- SCL GPIO18
- SDA GPIO17
- AXS15231B touch address 0x3B

TCA9554 bits:
- bit 0: TOUCH_INT
- bit 1: BL_EN
- bit 2: IMU_INT1
- bit 3: IMU_INT2
- bit 4: RTC_INT
- bit 5: LCD_RST
- bit 6: SYS_EN
- bit 7: NS_MODE

GPIO16 is the V2 `SYS_OUT` power-status input used by Waveshare's battery/power example.

## Touch transform

Waveshare's V2 reference maps the controller's raw coordinates to native LCD coordinates as:

```text
native_x = raw_y
native_y = 640 - raw_x
```

The project reproduces this in `esp_lcd_touch` with mirror-X + swap-XY. LVGL then rotates pointer coordinates together with the display, yielding the final 640 x 172 landscape interaction space.

All transform flags are centralized in `components/board/include/board_config.h`. The first physical firmware test must verify all four corners and swipe direction; if an axis is mirrored on the actual production unit, fix the board mapping only, never individual UI pages.

## Runtime ownership

`components/board/board.c` owns:
- system/touch I2C buses
- TCA9554
- QSPI LCD
- touch controller
- backlight PWM
- `esp_lvgl_port`
- display rotation
- common LVGL mutex access

UI pages must remain hardware-independent.

## Boot sequence

1. Backlight PWM starts at 0%.
2. System I2C and TCA9554 initialize.
3. BL_EN stays off; SYS_EN is asserted.
4. LCD resets through TCA9554.
5. QSPI AXS15231B panel initializes.
6. Touch I2C/controller initializes.
7. `esp_lvgl_port` starts and registers display/touch.
8. LVGL rotates the native panel 90 degrees.
9. `app_main()` acquires the LVGL mutex and calls `ui_init()`.
10. Backlight is enabled only after the first UI tree exists.

## First physical validation checklist

After a successful ESP-IDF build, test on the actual V2 board in this order:

1. no white/garbage flash during boot;
2. clock renders at the correct 640 x 172 orientation;
3. touch all four corners and center;
4. verify tap, horizontal drag and vertical transition directions;
5. run repeated page transitions for heap/stability observation;
6. sweep brightness from 10% to 100%;
7. leave the clock running for an extended period before adding Wi-Fi/HA/audio.

Audio, RTC, IMU, SD, battery ADC/charging state and Home Assistant are intentionally separate follow-up services. They should not be folded into the display BSP.
