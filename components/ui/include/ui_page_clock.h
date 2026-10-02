#pragma once
#include "lvgl.h"

typedef enum {
    UI_CLOCK_BATTERY_NORMAL = 0,
    UI_CLOCK_BATTERY_LOW,
    UI_CLOCK_BATTERY_CHARGING,
} ui_clock_battery_state_t;

void ui_page_clock_build(lv_obj_t *parent);
void ui_page_clock_stop(void);
void ui_page_clock_set_battery_state(ui_clock_battery_state_t state);
