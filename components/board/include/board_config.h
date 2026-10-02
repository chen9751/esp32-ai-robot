#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"

/*
 * Waveshare ESP32-S3-Touch-LCD-3.49 V2 hardware map.
 *
 * Keep the UI logical resolution independent from the panel's native wiring:
 * panel native = 172 x 640, project UI = 640 x 172 (90 degree rotation).
 * Values are aligned with Waveshare's ESP-IDF V2 examples.
 */

#define BOARD_LCD_HOST                  SPI3_HOST
#define BOARD_LCD_NATIVE_H_RES          172
#define BOARD_LCD_NATIVE_V_RES          640
#define BOARD_UI_H_RES                  640
#define BOARD_UI_V_RES                  172

#define BOARD_LCD_PIN_CS                GPIO_NUM_9
#define BOARD_LCD_PIN_PCLK              GPIO_NUM_10
#define BOARD_LCD_PIN_DATA0             GPIO_NUM_11
#define BOARD_LCD_PIN_DATA1             GPIO_NUM_12
#define BOARD_LCD_PIN_DATA2             GPIO_NUM_13
#define BOARD_LCD_PIN_DATA3             GPIO_NUM_14
#define BOARD_LCD_PIN_TE                GPIO_NUM_21
#define BOARD_BACKLIGHT_PIN             GPIO_NUM_42

#define BOARD_SYS_I2C_PORT              I2C_NUM_0
#define BOARD_SYS_I2C_SCL               GPIO_NUM_48
#define BOARD_SYS_I2C_SDA               GPIO_NUM_47

#define BOARD_TOUCH_I2C_PORT            I2C_NUM_1
#define BOARD_TOUCH_I2C_SCL             GPIO_NUM_18
#define BOARD_TOUCH_I2C_SDA             GPIO_NUM_17
#define BOARD_TOUCH_I2C_HZ              300000
#define BOARD_TOUCH_ADDR                0x3B

#define BOARD_EXIO_PIN_TOUCH_INT        (1ULL << 0)
#define BOARD_EXIO_PIN_BL_EN            (1ULL << 1)
#define BOARD_EXIO_PIN_IMU_INT1         (1ULL << 2)
#define BOARD_EXIO_PIN_IMU_INT2         (1ULL << 3)
#define BOARD_EXIO_PIN_RTC_INT          (1ULL << 4)
#define BOARD_EXIO_PIN_LCD_RST          (1ULL << 5)
#define BOARD_EXIO_PIN_SYS_EN           (1ULL << 6)
#define BOARD_EXIO_PIN_NS_MODE          (1ULL << 7)

#define BOARD_POWER_SYS_OUT_PIN         GPIO_NUM_16

/* Keep the LVGL draw buffers small and DMA capable; rotation is handled by
 * esp_lvgl_port in software. 64 native rows matches Waveshare's example. */
#define BOARD_LVGL_DRAW_BUF_PIXELS       (BOARD_LCD_NATIVE_H_RES * 64)

/*
 * AXS15231B reports the V2 touch panel in its raw 640 x 172 orientation.
 * Waveshare's own V2 example maps it to the native LCD coordinates as:
 *     native_x = raw_y
 *     native_y = 640 - raw_x
 * esp_lcd_touch applies mirror before swap, therefore mirror-X + swap-XY
 * reproduces that mapping. LVGL 9.2 then applies the display's 90 degree
 * rotation to produce the project's final 640 x 172 logical coordinates.
 */
#define BOARD_TOUCH_RAW_X_MAX           BOARD_LCD_NATIVE_V_RES
#define BOARD_TOUCH_RAW_Y_MAX           BOARD_LCD_NATIVE_H_RES
#define BOARD_TOUCH_SWAP_XY             1
#define BOARD_TOUCH_MIRROR_X            1
#define BOARD_TOUCH_MIRROR_Y            0

#define BOARD_BACKLIGHT_DEFAULT_PERCENT 60
