#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_STANDBY_CLOCK = 0,
    UI_STANDBY_WEATHER,
    UI_STANDBY_CALENDAR,
    UI_STANDBY_COUNT,
} ui_standby_view_t;

typedef enum {
    UI_STANDBY_EVENT_OPEN_HOME = 0,
    UI_STANDBY_EVENT_PREVIOUS,
    UI_STANDBY_EVENT_NEXT,
} ui_standby_event_t;

typedef void (*ui_standby_event_cb_t)(ui_standby_event_t event, void *user_data);

void ui_page_standby_show(ui_standby_view_t view,
                          bool show_swipe_hint,
                          ui_standby_event_cb_t event_cb,
                          void *event_user_data);

void ui_page_standby_stop(void);
