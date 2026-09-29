#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_REMOTE_ICON_POWER = 0,
    UI_REMOTE_ICON_INPUT,
    UI_REMOTE_ICON_HOME,
    UI_REMOTE_ICON_BACK,
    UI_REMOTE_ICON_SETTINGS,
    UI_REMOTE_ICON_DISPLAY,
    UI_REMOTE_ICON_ADD,
    UI_REMOTE_ICON_SUBTRACT,
} ui_remote_icon_t;

lv_obj_t *ui_remote_icon_create(lv_obj_t *parent, ui_remote_icon_t icon, lv_color_t color);

#ifdef __cplusplus
}
#endif
