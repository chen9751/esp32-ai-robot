#include "ui_page_settings.h"

#include <stdint.h>

#define UI_SCREEN_W              640
#define UI_SCREEN_H              172
#define UI_LEFT_RESERVED          56
#define UI_SETTINGS_X             64
#define UI_SETTINGS_W            568
#define UI_COL_W                 184
#define UI_COL_GAP                 8
#define UI_ICON_Y                 24
#define UI_SLIDER_Y              108
#define UI_SLIDER_W              138
#define UI_SLIDER_H                8
#define UI_ACCENT              lv_color_hex(0x45D7F0)
#define UI_TRACK               lv_color_hex(0x2A2D31)
#define UI_FG                  lv_color_hex(0xE9ECEF)
#define UI_MUTED               lv_color_hex(0x5A5F66)
#define UI_MUTED_SOFT          lv_color_hex(0x34383D)
#define UI_RED                 lv_color_hex(0xEF4444)

static ui_settings_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static lv_obj_t *s_root = NULL;

static void note_activity(void)
{
    if (s_activity_cb != NULL) {
        s_activity_cb(s_activity_user_data);
    }
}

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *bar(lv_obj_t *parent, int32_t w, int32_t h, lv_color_t color)
{
    lv_obj_t *obj = plain_obj(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, h / 2, 0);
    return obj;
}

static void create_separator(lv_obj_t *parent, int32_t x)
{
    lv_obj_t *top = bar(parent, 1, 34, UI_MUTED_SOFT);
    lv_obj_set_pos(top, x, 16);

    lv_obj_t *mid = bar(parent, 2, 54, UI_MUTED);
    lv_obj_set_pos(mid, x - 1, 59);

    lv_obj_t *bottom = bar(parent, 1, 34, UI_MUTED_SOFT);
    lv_obj_set_pos(bottom, x, 122);
}

static lv_obj_t *create_wifi_icon(lv_obj_t *parent)
{
    lv_obj_t *holder = plain_obj(parent);
    lv_obj_set_size(holder, 72, 62);

    const int sizes[] = {58, 42, 26};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *arc = lv_arc_create(holder);
        lv_obj_remove_style_all(arc);
        lv_obj_set_size(arc, sizes[i], sizes[i]);
        lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, i * 8);
        lv_arc_set_bg_angles(arc, 210, 330);
        lv_obj_set_style_arc_width(arc, 4, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, UI_FG, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *dot = bar(holder, 8, 8, UI_FG);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -1);
    return holder;
}

static void create_cross(lv_obj_t *parent)
{
    lv_obj_t *a = bar(parent, 14, 2, lv_color_hex(0xFFFFFF));
    lv_obj_center(a);
    lv_obj_set_style_transform_rotation(a, 450, 0);

    lv_obj_t *b = bar(parent, 14, 2, lv_color_hex(0xFFFFFF));
    lv_obj_center(b);
    lv_obj_set_style_transform_rotation(b, 1350, 0);
}

static lv_obj_t *create_wifi_status(lv_obj_t *parent)
{
    lv_obj_t *badge = plain_obj(parent);
    lv_obj_set_size(badge, 28, 28);
    lv_obj_set_style_bg_color(badge, UI_RED, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    create_cross(badge);
    return badge;
}

static lv_obj_t *create_sun_icon(lv_obj_t *parent)
{
    lv_obj_t *holder = plain_obj(parent);
    lv_obj_set_size(holder, 64, 64);

    lv_obj_t *core = plain_obj(holder);
    lv_obj_set_size(core, 28, 28);
    lv_obj_center(core);
    lv_obj_set_style_border_width(core, 2, 0);
    lv_obj_set_style_border_color(core, UI_FG, 0);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(core, LV_OBJ_FLAG_CLIP_CHILDREN);

    lv_obj_t *half = plain_obj(core);
    lv_obj_set_size(half, 14, 28);
    lv_obj_set_pos(half, 14, 0);
    lv_obj_set_style_bg_color(half, UI_FG, 0);
    lv_obj_set_style_bg_opa(half, LV_OPA_COVER, 0);

    static const int16_t ray_pos[8][4] = {
        {30, 2, 4, 10}, {30, 52, 4, 10},
        {2, 30, 10, 4}, {52, 30, 10, 4},
        {10, 10, 8, 3}, {46, 10, 8, 3},
        {10, 51, 8, 3}, {46, 51, 8, 3},
    };

    for (int i = 0; i < 8; ++i) {
        lv_obj_t *ray = bar(holder,
                            ray_pos[i][2],
                            ray_pos[i][3],
                            UI_FG);
        lv_obj_set_pos(ray, ray_pos[i][0], ray_pos[i][1]);
        if (i == 4 || i == 7) lv_obj_set_style_transform_rotation(ray, 450, 0);
        if (i == 5 || i == 6) lv_obj_set_style_transform_rotation(ray, 1350, 0);
    }

    return holder;
}

static lv_obj_t *create_speaker_icon(lv_obj_t *parent)
{
    lv_obj_t *holder = plain_obj(parent);
    lv_obj_set_size(holder, 64, 58);

    lv_obj_t *box = bar(holder, 15, 22, UI_FG);
    lv_obj_set_pos(box, 8, 18);
    lv_obj_set_style_radius(box, 3, 0);

    lv_obj_t *cone = plain_obj(holder);
    lv_obj_set_size(cone, 18, 32);
    lv_obj_set_pos(cone, 21, 13);
    lv_obj_set_style_bg_color(cone, UI_FG, 0);
    lv_obj_set_style_bg_opa(cone, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cone, 3, 0);

    lv_obj_t *wave1 = lv_arc_create(holder);
    lv_obj_remove_style_all(wave1);
    lv_obj_set_size(wave1, 28, 32);
    lv_obj_set_pos(wave1, 34, 13);
    lv_arc_set_bg_angles(wave1, 300, 60);
    lv_obj_set_style_arc_width(wave1, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(wave1, UI_FG, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(wave1, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(wave1, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(wave1, LV_OBJ_FLAG_CLICKABLE);

    return holder;
}

static void style_slider(lv_obj_t *slider)
{
    lv_obj_set_size(slider, UI_SLIDER_W, UI_SLIDER_H);
    lv_obj_set_style_bg_color(slider, UI_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    lv_obj_set_style_bg_color(slider, UI_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(slider, lv_color_hex(0xF4F7F8), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_width(slider, 18, LV_PART_KNOB);
    lv_obj_set_style_height(slider, 18, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
}

static void slider_event_cb(lv_event_t *e)
{
    (void)e;
    note_activity();
}

static void wifi_pressed_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_obj_set_style_translate_y(obj, 2, 0);
    note_activity();
}

static void wifi_released_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_obj_set_style_translate_y(obj, 0, 0);
    note_activity();
}

static lv_obj_t *create_column(lv_obj_t *parent, int index)
{
    lv_obj_t *col = plain_obj(parent);
    lv_obj_set_size(col, UI_COL_W, UI_SCREEN_H);
    lv_obj_set_pos(col,
                   UI_SETTINGS_X + index * (UI_COL_W + UI_COL_GAP),
                   0);
    return col;
}

lv_obj_t *ui_page_settings_build(lv_obj_t *parent,
                                 ui_settings_activity_cb_t activity_cb,
                                 void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;

    s_root = plain_obj(parent);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);

    lv_obj_t *wifi = create_column(s_root, 0);
    lv_obj_add_flag(wifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(wifi, wifi_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(wifi, wifi_released_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(wifi, wifi_released_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_t *wifi_icon = create_wifi_icon(wifi);
    lv_obj_align(wifi_icon, LV_ALIGN_TOP_MID, 0, UI_ICON_Y);
    lv_obj_t *wifi_status = create_wifi_status(wifi);
    lv_obj_align(wifi_status, LV_ALIGN_BOTTOM_MID, 0, -24);

    lv_obj_t *brightness = create_column(s_root, 1);
    lv_obj_t *sun = create_sun_icon(brightness);
    lv_obj_align(sun, LV_ALIGN_TOP_MID, 0, UI_ICON_Y - 2);
    lv_obj_t *brightness_slider = lv_slider_create(brightness);
    style_slider(brightness_slider);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, UI_SLIDER_Y);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, 62, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *volume = create_column(s_root, 2);
    lv_obj_t *speaker = create_speaker_icon(volume);
    lv_obj_align(speaker, LV_ALIGN_TOP_MID, 0, UI_ICON_Y + 1);
    lv_obj_t *volume_slider = lv_slider_create(volume);
    style_slider(volume_slider);
    lv_obj_align(volume_slider, LV_ALIGN_TOP_MID, 0, UI_SLIDER_Y);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, 72, LV_ANIM_OFF);
    lv_obj_add_event_cb(volume_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    create_separator(s_root, UI_SETTINGS_X + UI_COL_W + UI_COL_GAP / 2);
    create_separator(s_root, UI_SETTINGS_X + (UI_COL_W + UI_COL_GAP) * 2 - UI_COL_GAP / 2);

    return s_root;
}

void ui_page_settings_stop(void)
{
    s_root = NULL;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}
