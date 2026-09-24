# Waveshare V2 hardware port plan

The physical board path is intentionally based on the verified Waveshare ESP-IDF LVGL v9 reference rather than guessed GPIO.

Official reference currently uses:
- AXS15231B QSPI LCD, 172x640 native
- SPI3: CS 9, PCLK 10, DATA0..3 = 11..14, reset 21
- Backlight GPIO 8
- Touch I2C SDA 17 / SCL 18, address 0x3B
- LVGL 9 and espressif/esp_lcd_axs15231b
- touch coordinate conversion in the LVGL input callback

The project UI is kept independent from this board port. Before copying the reference driver into `components/board`, compare it against the V2 source package used in the successful hardware test. Do not substitute V1 pin mappings.

Next hardware integration:
1. import the exact V2 LCD/touch/backlight BSP used by the successful test;
2. expose board_display_start(), board_lvgl_lock/unlock(), board_set_brightness();
3. call ui_init() only after display + input registration;
4. verify landscape orientation and touch coordinates on-device;
5. then bind ui_action_brightness() to the PWM backlight.
