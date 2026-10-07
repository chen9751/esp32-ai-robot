#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the Waveshare ESP32-S3-Touch-LCD-3.49 V2 display, touch,
 * backlight power path and the ESP LVGL runtime. The backlight remains at
 * 0% until the application has built the first UI frame. */
esp_err_t board_init(void);

bool board_display_ready(void);

/* LVGL is not thread-safe. Any LVGL call made outside LVGL event/timer
 * callbacks must use this same lock. timeout_ms == 0 means wait forever. */
bool board_display_lock(uint32_t timeout_ms);
void board_display_unlock(void);

esp_err_t board_backlight_set_percent(uint8_t percent);
uint8_t board_backlight_get_percent(void);

typedef struct {
    bool available;
    bool battery_present;
    bool external_power;
    bool charging;
    bool low_battery;
    uint16_t battery_mv;
} board_power_status_t;

/* Real Waveshare V2 power telemetry:
 * VBAT from GPIO4/ADC1_CH3 and external-vs-battery source from GPIO16 SYS_OUT.
 * "charging" denotes external power with a detected battery; the board does
 * not expose a dedicated charger-current status through the reference API. */
esp_err_t board_power_get_status(board_power_status_t *status);

/* Software power-off path used by the physical PWR key. On battery power this
 * drops Waveshare's EXIO6/SYS_EN rail. USB-powered operation may remain alive
 * electrically, so the application uses deep sleep as a fallback. */
esp_err_t board_power_off(void);

typedef enum {
    BOARD_ORIENTATION_UNKNOWN = 0,
    BOARD_ORIENTATION_LANDSCAPE,
    BOARD_ORIENTATION_PORTRAIT_RIGHT,
    BOARD_ORIENTATION_LANDSCAPE_INVERTED,
    BOARD_ORIENTATION_PORTRAIT_LEFT,
} board_orientation_t;

/* QMI8658-backed gravity orientation. When the board is nearly flat, UNKNOWN
 * is returned so callers can keep their last stable orientation. */
esp_err_t board_imu_get_orientation(board_orientation_t *orientation);

/* Bring-up helper: bypass LVGL and write solid RGB565 frames directly to the
 * AXS15231B. Intended only for real-hardware display diagnostics. */
esp_err_t board_display_run_color_test(void);

#ifdef __cplusplus
}
#endif
