#include "ui_page_settings.h"
#include "ui_system_icons.h"

#include <stdint.h>

#define UI_SCREEN_W              640
#define UI_SCREEN_H              172

/* 56 px global back rail + 584 px settings content. */
#define UI_WIFI_X                 56
#define UI_WIFI_W                128
#define UI_GAP                    12
#define UI_CONTROL_W             216
#define UI_BRIGHT_X             (UI_WIFI_X + UI_WIFI_W + UI_GAP)
#define UI_VOLUME_X             (UI_BRIGHT_X + UI_CONTROL_W + UI_GAP)

#define UI_ICON_HOLDER_W          44
#define UI_ICON_HOLDER_H          44
#define UI_ICON_TOP_Y             24
#define UI_WIFI_ICON_TOP_Y        58
#define UI_VALUE_Y                76
#define UI_SLIDER_Y              111
#define UI_SLIDER_W              184
#define UI_SLIDER_H                8

#define UI_ACCENT              lv_color_hex(0x45D7F0)
#define UI_TRACK               lv_color_hex(0x2A2D31)
#define UI_FG                  lv_color_hex(0xE9ECEF)
#define UI_WIFI_OFF            lv_color_hex(0x555A60)

static ui_settings_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static lv_obj_t *s_root = NULL;
static lv_obj_t *s_wifi_icon = NULL;
static bool s_wifi_connected = false;

typedef struct {
    lv_obj_t *slider;
    lv_obj_t *value_label;
    int32_t min_value;
    int32_t max_value;
    int32_t step;
    bool adjusting;
} slider_ctx_t;

static slider_ctx_t s_brightness_ctx = {0};
static slider_ctx_t s_volume_ctx = {0};

static void note_activity(void)
{
    if (s_activity_cb != NULL) s_activity_cb(s_activity_user_data);
}

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *create_column(lv_obj_t *parent, int32_t x, int32_t w)
{
    lv_obj_t *col = plain_obj(parent);
    lv_obj_set_size(col, w, UI_SCREEN_H);
    lv_obj_set_pos(col, x, 0);
    return col;
}

static lv_obj_t *create_icon_holder(lv_obj_t *parent, int32_t top_y)
{
    lv_obj_t *holder = plain_obj(parent);
    lv_obj_set_size(holder, UI_ICON_HOLDER_W, UI_ICON_HOLDER_H);
    lv_obj_align(holder, LV_ALIGN_TOP_MID, 0, top_y);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_CLICKABLE);
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

static int32_t snap_value(int32_t value, int32_t min_value, int32_t max_value, int32_t step)
{
    int32_t relative = value - min_value;
    int32_t snapped = min_value + ((relative + step / 2) / step) * step;
    if (snapped < min_value) snapped = min_value;
    if (snapped > max_value) snapped = max_value;
    return snapped;
}

static void update_slider_label(slider_ctx_t *ctx)
{
    int32_t value = lv_slider_get_value(ctx->slider);
    lv_label_set_text_fmt(ctx->value_label, "%ld%%", (long)value);
}

static void slider_event_cb(lv_event_t *e)
{
    slider_ctx_t *ctx = (slider_ctx_t *)lv_event_get_user_data(e);
    if (ctx == NULL || ctx->slider == NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    note_activity();

    if (code == LV_EVENT_PRESSED) {
        lv_obj_clear_flag(ctx->value_label, LV_OBJ_FLAG_HIDDEN);
        update_slider_label(ctx);
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        int32_t raw = lv_slider_get_value(ctx->slider);
        int32_t snapped = snap_value(raw, ctx->min_value, ctx->max_value, ctx->step);

        if (!ctx->adjusting && raw != snapped) {
            ctx->adjusting = true;
            lv_slider_set_value(ctx->slider, snapped, LV_ANIM_OFF);
            ctx->adjusting = false;
        }

        lv_obj_clear_flag(ctx->value_label, LV_OBJ_FLAG_HIDDEN);
        update_slider_label(ctx);
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_add_flag(ctx->value_label, LV_OBJ_FLAG_HIDDEN);
    }
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

static lv_obj_t *create_value_label(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_color(label, UI_FG, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, UI_VALUE_Y);
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    return label;
}

static void build_slider_control(lv_obj_t *column,
                                 lv_obj_t *icon,
                                 slider_ctx_t *ctx,
                                 int32_t min_value,
                                 int32_t max_value,
                                 int32_t step,
                                 int32_t initial_value)
{
    lv_obj_center(icon);

    ctx->value_label = create_value_label(column);
    ctx->slider = lv_slider_create(column);
    ctx->min_value = min_value;
    ctx->max_value = max_value;
    ctx->step = step;
    ctx->adjusting = false;

    style_slider(ctx->slider);
    lv_obj_align(ctx->slider, LV_ALIGN_TOP_MID, 0, UI_SLIDER_Y);
    lv_slider_set_range(ctx->slider, min_value, max_value);
    lv_slider_set_value(ctx->slider,
                        snap_value(initial_value, min_value, max_value, step),
                        LV_ANIM_OFF);

    lv_obj_add_event_cb(ctx->slider, slider_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_add_event_cb(ctx->slider, slider_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->slider, slider_event_cb, LV_EVENT_PRESS_LOST, ctx);
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

    /*
     * No separator lines: the three groups are separated only by 12 px
     * negative-space gutters.  Widths are exact so every icon/slider is
     * mathematically centered in its own region.
     */
    lv_obj_t *wifi = create_column(s_root, UI_WIFI_X, UI_WIFI_W);
    lv_obj_add_flag(wifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(wifi, wifi_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(wifi, wifi_released_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(wifi, wifi_released_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_t *wifi_holder = create_icon_holder(wifi, UI_WIFI_ICON_TOP_Y);
    s_wifi_icon = ui_system_icon_wifi(wifi_holder,
                                      s_wifi_connected ? UI_FG : UI_WIFI_OFF);
    lv_obj_center(s_wifi_icon);

    lv_obj_t *brightness = create_column(s_root, UI_BRIGHT_X, UI_CONTROL_W);
    lv_obj_t *brightness_holder = create_icon_holder(brightness, UI_ICON_TOP_Y);
    lv_obj_t *sun = ui_system_icon_brightness(brightness_holder, UI_FG);
    build_slider_control(brightness,
                         sun,
                         &s_brightness_ctx,
                         10,
                         100,
                         10,
                         60);

    lv_obj_t *volume = create_column(s_root, UI_VOLUME_X, UI_CONTROL_W);
    lv_obj_t *volume_holder = create_icon_holder(volume, UI_ICON_TOP_Y);
    lv_obj_t *speaker = ui_system_icon_volume(volume_holder, UI_FG);
    build_slider_control(volume,
                         speaker,
                         &s_volume_ctx,
                         0,
                         100,
                         5,
                         70);

    return s_root;
}

void ui_page_settings_set_wifi_connected(bool connected)
{
    s_wifi_connected = connected;
    if (s_wifi_icon != NULL) {
        ui_system_icon_set_color(s_wifi_icon,
                                 connected ? UI_FG : UI_WIFI_OFF);
    }
}

void ui_page_settings_stop(void)
{
    s_root = NULL;
    s_wifi_icon = NULL;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
    s_brightness_ctx = (slider_ctx_t){0};
    s_volume_ctx = (slider_ctx_t){0};
}
