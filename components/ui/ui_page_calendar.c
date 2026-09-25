#include "ui_page_calendar.h"

void ui_page_calendar_build(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "CALENDAR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x7A7A7A), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -8);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *subtitle = lv_label_create(parent);
    lv_label_set_text(subtitle, "PLACEHOLDER");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x383838), 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 16);
    lv_obj_clear_flag(subtitle, LV_OBJ_FLAG_CLICKABLE);
}
