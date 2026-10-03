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

/* Bring-up helper: bypass LVGL and write solid RGB565 frames directly to the
 * AXS15231B. Intended only for real-hardware display diagnostics. */
esp_err_t board_display_run_color_test(void);

#ifdef __cplusplus
}
#endif
