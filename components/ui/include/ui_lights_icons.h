#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_LIGHTS_ICON_SOFA = 0,
    UI_LIGHTS_ICON_COMPUTER,
    UI_LIGHTS_ICON_BED,
    UI_LIGHTS_ICON_BULB,
    UI_LIGHTS_ICON_TV,
    UI_LIGHTS_ICON_DROP,
    UI_LIGHTS_ICON_WINDOW,
    UI_LIGHTS_ICON_BRIGHTNESS,
    UI_LIGHTS_ICON_TEMPERATURE,
    UI_LIGHTS_ICON_PALETTE,
} ui_lights_icon_t;

/* Project-approved Remix Icon assets. Do not replace with hand-drawn LVGL geometry. */
lv_obj_t *ui_lights_icon_create(lv_obj_t *parent, ui_lights_icon_t type, lv_color_t color);
void ui_lights_icon_set_color(lv_obj_t *icon, lv_color_t color);

#ifdef __cplusplus
}
#endif
