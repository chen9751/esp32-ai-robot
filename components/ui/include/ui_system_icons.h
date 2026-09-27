#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Shared system-setting icons sourced from the project-approved Remix Icon
 * family. Page code should not draw one-off replacement icon geometry.
 */
lv_obj_t *ui_system_icon_volume(lv_obj_t *parent, lv_color_t color);
lv_obj_t *ui_system_icon_brightness(lv_obj_t *parent, lv_color_t color);
lv_obj_t *ui_system_icon_wifi(lv_obj_t *parent, lv_color_t color);
lv_obj_t *ui_system_icon_bluetooth(lv_obj_t *parent, lv_color_t color);
lv_obj_t *ui_system_icon_ai(lv_obj_t *parent, lv_color_t color);
lv_obj_t *ui_system_icon_system(lv_obj_t *parent, lv_color_t color);

void ui_system_icon_set_color(lv_obj_t *icon, lv_color_t color);

#ifdef __cplusplus
}
#endif
