#include "ui_page_lights.h"
#include "ha_lights.h"
#include "ui_lights_labels.h"
#include "ui_lights_icons.h"
#include "ui_lights_extra_icons.h"

#include <stddef.h>
#include <stdint.h>

#ifdef UI_LIGHTS_HAS_SOURCE_HAN
LV_FONT_DECLARE(ui_font_source_han_lights_18);
#endif

#define UI_SCREEN_W 640
#define UI_SCREEN_H 172
#define ITEM_W 142
#define ITEM_H 158
#define LEFT_PAD 50
#define RIGHT_PAD 10
#define GAP 4
#define FOOTER_Y 108
#define FOOTER_H 50
#define ADJUST_TIMEOUT_MS 30000

#define BRIGHTNESS_MIN 1
#define BRIGHTNESS_MAX 100
#define BRIGHTNESS_STEP 1

#define TEMP_MIN_K 2500
#define TEMP_MAX_K 6500
#define TEMP_STEP_K 100

#define COLOR_MIN 0
#define COLOR_MAX 100
#define COLOR_STEP 1

#define C_BG            lv_color_hex(0x000000)
#define C_CARD          lv_color_hex(0x090A0C)
#define C_CARD_ON       lv_color_hex(0x101113)
#define C_FOOTER        lv_color_hex(0x0D0E10)
#define C_TEXT          lv_color_hex(0xF5F5F7)
#define C_OFF           lv_color_hex(0x686C73)
#define C_OFF_DARK      lv_color_hex(0x24262A)
#define C_DIVIDER       lv_color_hex(0x23252A)
#define C_WARM          lv_color_hex(0xFFD06F)
#define C_WARM_HI       lv_color_hex(0xFFF0C2)
#define C_TEMP_WARM     lv_color_hex(0xFFD36A)
#define C_TEMP_COOL     lv_color_hex(0xFFFFFF)
#define C_SWITCH_ON     lv_color_hex(0x3A301F)
#define C_ADJUST_ACTIVE lv_color_hex(0x2A2317)

typedef enum { LIGHT_NORMAL=0, LIGHT_RGB, LIGHT_SWITCH_ONLY } light_kind_t;
typedef enum { ADJUST_NONE=0, ADJUST_BRIGHTNESS, ADJUST_TEMPERATURE, ADJUST_COLOR } adjust_mode_t;

typedef struct {
    const char *name;
    const lv_image_dsc_t *fallback_label;
    ui_lights_icon_t icon;
    const lv_image_dsc_t *extra_icon;
    light_kind_t kind;
    bool initial_on;
} light_spec_t;

typedef struct {
    lv_obj_t *item;
    lv_obj_t *label;
    lv_obj_t *accent;
    lv_obj_t *room_icon;
    lv_obj_t *room_detail;
    lv_obj_t *footer;
    lv_obj_t *controls[3];
    lv_obj_t *control_icons[3];
    lv_obj_t *switch_dot;
    lv_obj_t *adjust_panel;
    lv_obj_t *adjust_slider;
    lv_obj_t *adjust_value;
    lv_obj_t *hue_track;
    lv_obj_t *hue_slider;
    lv_obj_t *saturation_slider;
    light_kind_t kind;
    adjust_mode_t adjust_mode;
    bool on;
    uint8_t brightness;
    uint16_t color_temp_k;
    uint8_t hue_percent;
    uint8_t saturation;
} light_view_t;

static const light_spec_t SPECS[8] = {
    { "客厅灯",   &ui_lights_label_living,        UI_LIGHTS_ICON_SOFA,     NULL,                     LIGHT_NORMAL,      true  },
    { "书房灯",   &ui_lights_label_study,         UI_LIGHTS_ICON_COMPUTER, &ui_lights_extra_mac,     LIGHT_NORMAL,      false },
    { "卧室灯",   &ui_lights_label_bedroom,       UI_LIGHTS_ICON_BED,      NULL,                     LIGHT_NORMAL,      true  },
    { "床头灯",   &ui_lights_label_bedside,       UI_LIGHTS_ICON_BULB,     NULL,                     LIGHT_RGB,         true  },
    { "小卧室灯", &ui_lights_label_small_bedroom, UI_LIGHTS_ICON_BED,      &ui_lights_extra_gamepad, LIGHT_NORMAL,      false },
    { "彩光灯带", &ui_lights_label_rgb_strip,     UI_LIGHTS_ICON_TV,       &ui_lights_extra_tv2,     LIGHT_RGB,         true  },
    { "浴室灯",   &ui_lights_label_bathroom,      UI_LIGHTS_ICON_DROP,     NULL,                     LIGHT_SWITCH_ONLY, false },
    { "阳台灯",   &ui_lights_label_balcony,       UI_LIGHTS_ICON_WINDOW,   NULL,                     LIGHT_SWITCH_ONLY, true  },
};

#define LIGHT_COUNT (sizeof(SPECS) / sizeof(SPECS[0]))

_Static_assert(UI_SCREEN_W == 640 && UI_SCREEN_H == 172,
               "Lights UI must stay on the project 640x172 landscape canvas");
_Static_assert(FOOTER_Y + FOOTER_H == ITEM_H,
               "Lights footer must exactly terminate at the card bottom");
_Static_assert((BRIGHTNESS_MAX - BRIGHTNESS_MIN) % BRIGHTNESS_STEP == 0,
               "Brightness range must align to its step");
_Static_assert((TEMP_MAX_K - TEMP_MIN_K) % TEMP_STEP_K == 0,
               "Color-temperature range must align to its step");
_Static_assert((COLOR_MAX - COLOR_MIN) % COLOR_STEP == 0,
               "Color range must align to its step");

static lv_obj_t *s_root;
static ui_lights_activity_cb_t s_activity_cb;
static void *s_activity_user_data;
static light_view_t s_views[LIGHT_COUNT];
#ifndef UI_LIGHTS_HAS_SOURCE_HAN
static bool s_labels_ready;
#endif
static light_view_t *s_adjust_view;
static lv_timer_t *s_adjust_timer;
static lv_timer_t *s_ha_timer;
static bool s_ha_initialized[LIGHT_COUNT];
/* These are the actual HA supported color-temperature limits per UI index. */
static const uint16_t s_min_kelvin[LIGHT_COUNT] = {2700,2700,3000,1700,3000,2700,0,0};
static const uint16_t s_max_kelvin[LIGHT_COUNT] = {6500,6500,5700,6500,5700,6500,0,0};

static int light_index(const light_view_t *view)
{
    return (int)(view - s_views);
}

static void note_activity(void)
{
    if (s_activity_cb) s_activity_cb(s_activity_user_data);
}

/* Geometry below is only chrome/layout. Semantic glyphs are approved Remix Icon assets. */
static lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h, int radius, lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static ui_lights_icon_t control_icon_type(int kind)
{
    if (kind == 0) return UI_LIGHTS_ICON_BRIGHTNESS;
    if (kind == 1) return UI_LIGHTS_ICON_TEMPERATURE;
    return UI_LIGHTS_ICON_PALETTE;
}

static lv_obj_t *create_text_or_bitmap_label(lv_obj_t *parent, const light_spec_t *spec)
{
#ifdef UI_LIGHTS_HAS_SOURCE_HAN
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, spec->name);
    lv_obj_set_style_text_font(label, &ui_font_source_han_lights_18, 0);
    lv_obj_set_style_text_color(label, C_TEXT, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 7);
#else
    lv_obj_t *label = lv_image_create(parent);
    lv_image_set_src(label, spec->fallback_label);
    lv_obj_set_style_image_recolor(label, C_TEXT, 0);
    lv_obj_set_style_image_recolor_opa(label, LV_OPA_COVER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 7);
#endif
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static lv_obj_t *create_room_icon(lv_obj_t *parent, const light_spec_t *spec, lv_color_t color)
{
    if (!spec->extra_icon) return ui_lights_icon_create(parent, spec->icon, color);

    lv_obj_t *icon = lv_image_create(parent);
    lv_image_set_src(icon, spec->extra_icon);
    lv_obj_set_style_image_recolor(icon, color, 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    return icon;
}

static void set_default_content_visible(light_view_t *view, bool visible)
{
    if (!view) return;
    if (visible) {
        lv_obj_clear_flag(view->label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->accent, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->room_icon, LV_OBJ_FLAG_HIDDEN);
        if (view->room_detail) lv_obj_clear_flag(view->room_detail, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(view->label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view->accent, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view->room_icon, LV_OBJ_FLAG_HIDDEN);
        if (view->room_detail) lv_obj_add_flag(view->room_detail, LV_OBJ_FLAG_HIDDEN);
    }
}

static int32_t snap_brightness(int32_t value)
{
    if (value < BRIGHTNESS_MIN) value = BRIGHTNESS_MIN;
    if (value > BRIGHTNESS_MAX) value = BRIGHTNESS_MAX;
    value = ((value + BRIGHTNESS_STEP / 2) / BRIGHTNESS_STEP) * BRIGHTNESS_STEP;
    if (value > BRIGHTNESS_MAX) value = BRIGHTNESS_MAX;
    return value;
}

static int32_t snap_color_temp(int32_t value)
{
    if (value < TEMP_MIN_K) value = TEMP_MIN_K;
    if (value > TEMP_MAX_K) value = TEMP_MAX_K;
    value = ((value + TEMP_STEP_K / 2) / TEMP_STEP_K) * TEMP_STEP_K;
    if (value > TEMP_MAX_K) value = TEMP_MAX_K;
    return value;
}

static int32_t snap_color_percent(int32_t value)
{
    if (value < COLOR_MIN) value = COLOR_MIN;
    if (value > COLOR_MAX) value = COLOR_MAX;
    value = ((value + COLOR_STEP / 2) / COLOR_STEP) * COLOR_STEP;
    if (value > COLOR_MAX) value = COLOR_MAX;
    return value;
}

static lv_color_t selected_hue_color(const light_view_t *view)
{
    uint16_t hue_deg = (uint16_t)((uint16_t)view->hue_percent * 360U / 100U);
    if (hue_deg >= 360U) hue_deg = 0U;
    return lv_color_hsv_to_rgb(hue_deg, 100, 100);
}

static void update_adjust_value(light_view_t *view)
{
    if (!view || !view->adjust_value) return;
    if (view->adjust_mode == ADJUST_BRIGHTNESS) {
        lv_label_set_text_fmt(view->adjust_value, "%u%%", (unsigned)view->brightness);
    } else if (view->adjust_mode == ADJUST_TEMPERATURE) {
        lv_label_set_text_fmt(view->adjust_value, "%uK", (unsigned)view->color_temp_k);
    }
}

static void stop_adjust_timer(void)
{
    if (!s_adjust_timer) return;
    lv_timer_delete(s_adjust_timer);
    s_adjust_timer = NULL;
}

static void clear_control_highlights(light_view_t *view)
{
    if (!view) return;
    for (int i = 0; i < 3; i++) {
        if (view->controls[i]) lv_obj_set_style_bg_opa(view->controls[i], LV_OPA_TRANSP, 0);
    }
}

static void set_color_controls_visible(light_view_t *view, bool visible)
{
    if (!view || !view->hue_track || !view->hue_slider || !view->saturation_slider) return;
    if (visible) {
        lv_obj_clear_flag(view->hue_track, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->hue_slider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->saturation_slider, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(view->hue_track, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view->hue_slider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view->saturation_slider, LV_OBJ_FLAG_HIDDEN);
    }
}

static void exit_adjust(light_view_t *view)
{
    if (!view || view->adjust_mode == ADJUST_NONE) return;

    view->adjust_mode = ADJUST_NONE;
    lv_obj_add_flag(view->adjust_panel, LV_OBJ_FLAG_HIDDEN);
    set_default_content_visible(view, true);
    set_color_controls_visible(view, false);
    clear_control_highlights(view);

    if (s_adjust_view == view) s_adjust_view = NULL;
    stop_adjust_timer();
}

static void adjust_timeout_cb(lv_timer_t *timer)
{
    light_view_t *view = (light_view_t *)lv_timer_get_user_data(timer);
    s_adjust_timer = NULL;
    if (view) exit_adjust(view);
}

static void restart_adjust_timer(light_view_t *view)
{
    stop_adjust_timer();
    s_adjust_timer = lv_timer_create(adjust_timeout_cb, ADJUST_TIMEOUT_MS, view);
    if (!s_adjust_timer) return;
    lv_timer_set_repeat_count(s_adjust_timer, 1);
    lv_timer_set_auto_delete(s_adjust_timer, true);
}

static void style_single_adjust_slider(light_view_t *view)
{
    if (!view || !view->adjust_slider) return;

    lv_obj_set_style_bg_opa(view->adjust_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(view->adjust_slider, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(view->adjust_slider, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(view->adjust_slider, C_WARM_HI, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(view->adjust_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_width(view->adjust_slider, 20, LV_PART_KNOB);
    lv_obj_set_style_height(view->adjust_slider, 20, LV_PART_KNOB);
    lv_obj_set_style_radius(view->adjust_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(view->adjust_slider, 6, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(view->adjust_slider, LV_OPA_30, LV_PART_KNOB);

    if (view->adjust_mode == ADJUST_TEMPERATURE) {
        lv_obj_set_style_bg_color(view->adjust_slider, C_TEMP_WARM, LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(view->adjust_slider, C_TEMP_COOL, LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(view->adjust_slider, LV_GRAD_DIR_HOR, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(view->adjust_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_color(view->adjust_slider, C_TEMP_WARM, LV_PART_KNOB);
    } else {
        lv_obj_set_style_bg_color(view->adjust_slider, C_OFF_DARK, LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(view->adjust_slider, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(view->adjust_slider, C_WARM, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(view->adjust_slider, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_color(view->adjust_slider, C_WARM, LV_PART_KNOB);
    }
}

static void update_saturation_gradient(light_view_t *view)
{
    if (!view || !view->saturation_slider) return;
    lv_color_t hue = selected_hue_color(view);
    lv_obj_set_style_bg_color(view->saturation_slider, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(view->saturation_slider, hue, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(view->saturation_slider, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(view->saturation_slider, hue, LV_PART_KNOB);
}

static void enter_adjust(light_view_t *view, adjust_mode_t mode)
{
    if (!view || mode == ADJUST_NONE) return;
    if ((mode == ADJUST_BRIGHTNESS && !view->controls[0]) ||
        (mode == ADJUST_TEMPERATURE && !view->controls[1]) ||
        (mode == ADJUST_COLOR && (view->kind != LIGHT_RGB || !view->controls[2]))) return;

    if (s_adjust_view && s_adjust_view != view) exit_adjust(s_adjust_view);

    view->adjust_mode = mode;
    s_adjust_view = view;
    set_default_content_visible(view, false);
    lv_obj_clear_flag(view->adjust_panel, LV_OBJ_FLAG_HIDDEN);
    clear_control_highlights(view);

    if (mode == ADJUST_BRIGHTNESS) {
        lv_obj_clear_flag(view->adjust_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->adjust_slider, LV_OBJ_FLAG_HIDDEN);
        set_color_controls_visible(view, false);
        lv_slider_set_range(view->adjust_slider, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
        lv_slider_set_value(view->adjust_slider, view->brightness, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(view->controls[0], C_ADJUST_ACTIVE, 0);
        lv_obj_set_style_bg_opa(view->controls[0], LV_OPA_COVER, 0);
        style_single_adjust_slider(view);
        update_adjust_value(view);
    } else if (mode == ADJUST_TEMPERATURE) {
        lv_obj_clear_flag(view->adjust_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->adjust_slider, LV_OBJ_FLAG_HIDDEN);
        set_color_controls_visible(view, false);
        int i = light_index(view);
        lv_slider_set_range(view->adjust_slider, s_min_kelvin[i], s_max_kelvin[i]);
        lv_slider_set_value(view->adjust_slider, view->color_temp_k, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(view->controls[1], C_ADJUST_ACTIVE, 0);
        lv_obj_set_style_bg_opa(view->controls[1], LV_OPA_COVER, 0);
        style_single_adjust_slider(view);
        update_adjust_value(view);
    } else {
        lv_obj_add_flag(view->adjust_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view->adjust_slider, LV_OBJ_FLAG_HIDDEN);
        set_color_controls_visible(view, true);
        lv_slider_set_value(view->hue_slider, view->hue_percent, LV_ANIM_OFF);
        lv_slider_set_value(view->saturation_slider, view->saturation, LV_ANIM_OFF);
        update_saturation_gradient(view);
        lv_obj_set_style_bg_color(view->controls[2], C_ADJUST_ACTIVE, 0);
        lv_obj_set_style_bg_opa(view->controls[2], LV_OPA_COVER, 0);
    }

    restart_adjust_timer(view);
    note_activity();
}

static void adjustment_control_click(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(event);
    if (!view || !target) return;

    adjust_mode_t requested = ADJUST_NONE;
    if (target == view->controls[0]) requested = ADJUST_BRIGHTNESS;
    else if (target == view->controls[1]) requested = ADJUST_TEMPERATURE;
    else if (target == view->controls[2] && view->kind == LIGHT_RGB) requested = ADJUST_COLOR;
    if (requested == ADJUST_NONE) return;

    if (view->adjust_mode == requested) exit_adjust(view);
    else enter_adjust(view, requested);

    note_activity();
}

static void adjust_slider_changed(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(event);
    if (!view || !slider || view->adjust_mode == ADJUST_NONE || view->adjust_mode == ADJUST_COLOR) return;

    int32_t raw = lv_slider_get_value(slider);
    if (view->adjust_mode == ADJUST_BRIGHTNESS) {
        int32_t snapped = snap_brightness(raw);
        view->brightness = (uint8_t)snapped;
        if (snapped != raw) lv_slider_set_value(slider, snapped, LV_ANIM_OFF);
    } else {
        int32_t snapped = snap_color_temp(raw);
        view->color_temp_k = (uint16_t)snapped;
        if (snapped != raw) lv_slider_set_value(slider, snapped, LV_ANIM_OFF);
    }

    update_adjust_value(view);
    restart_adjust_timer(view);
    note_activity();
}

static void adjust_slider_released(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    if (!view) return;
    int i = light_index(view);
    if (view->adjust_mode == ADJUST_BRIGHTNESS) {
        (void)ha_lights_send(i, HA_LIGHT_BRIGHTNESS, view->brightness, 0);
    } else if (view->adjust_mode == ADJUST_TEMPERATURE) {
        int k = view->color_temp_k;
        if (s_min_kelvin[i] && k < s_min_kelvin[i]) k = s_min_kelvin[i];
        if (s_max_kelvin[i] && k > s_max_kelvin[i]) k = s_max_kelvin[i];
        view->color_temp_k = (uint16_t)k;
        update_adjust_value(view);
        (void)ha_lights_send(i, HA_LIGHT_TEMPERATURE, k, 0);
    }
}

static void color_slider_released(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    if (!view) return;
    int hue = view->hue_percent * 360 / 100;
    if (hue >= 360) hue = 0;
    (void)ha_lights_send(light_index(view), HA_LIGHT_COLOR, hue, view->saturation);
}

static void hue_slider_changed(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(event);
    if (!view || !slider || view->adjust_mode != ADJUST_COLOR) return;

    int32_t raw = lv_slider_get_value(slider);
    int32_t snapped = snap_color_percent(raw);
    view->hue_percent = (uint8_t)snapped;
    if (snapped != raw) lv_slider_set_value(slider, snapped, LV_ANIM_OFF);
    update_saturation_gradient(view);
    restart_adjust_timer(view);
    note_activity();
}

static void saturation_slider_changed(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(event);
    if (!view || !slider || view->adjust_mode != ADJUST_COLOR) return;

    int32_t raw = lv_slider_get_value(slider);
    int32_t snapped = snap_color_percent(raw);
    view->saturation = (uint8_t)snapped;
    if (snapped != raw) lv_slider_set_value(slider, snapped, LV_ANIM_OFF);
    restart_adjust_timer(view);
    note_activity();
}

static lv_obj_t *make_control(lv_obj_t *footer, int x, int w, int kind, lv_obj_t **icon_out)
{
    lv_obj_t *touch = lv_obj_create(footer);
    lv_obj_remove_style_all(touch);
    lv_obj_set_pos(touch, x, 0);
    lv_obj_set_size(touch, w, FOOTER_H);
    lv_obj_add_flag(touch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(touch, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(touch, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = ui_lights_icon_create(touch, control_icon_type(kind), C_TEXT);
    lv_obj_center(icon);
    if (icon_out) *icon_out = icon;
    return touch;
}

static void add_vertical_divider(lv_obj_t *footer, int x)
{
    rect(footer, x, 10, 1, 30, 0, C_DIVIDER);
}

static void create_footer(light_view_t *view)
{
    view->footer = rect(view->item, 0, FOOTER_Y, ITEM_W, FOOTER_H, 0, C_FOOTER);
    rect(view->item, 0, FOOTER_Y, ITEM_W, 1, 0, C_DIVIDER);

    if (view->kind == LIGHT_SWITCH_ONLY) {
        lv_obj_t *track = rect(view->footer, 44, 11, 54, 28, 14, C_OFF_DARK);
        view->switch_dot = rect(track, 4, 4, 20, 20, 10, C_OFF);
        return;
    }

    if (view->kind == LIGHT_NORMAL) {
        const int half = ITEM_W / 2;
        view->controls[0] = make_control(view->footer, 0, half, 0, &view->control_icons[0]);
        view->controls[1] = make_control(view->footer, half, ITEM_W - half, 1, &view->control_icons[1]);
        add_vertical_divider(view->footer, half);
    } else {
        const int third = ITEM_W / 3;
        view->controls[0] = make_control(view->footer, 0, third, 0, &view->control_icons[0]);
        view->controls[1] = make_control(view->footer, third, third, 1, &view->control_icons[1]);
        view->controls[2] = make_control(view->footer, third * 2, ITEM_W - third * 2, 2, &view->control_icons[2]);
        add_vertical_divider(view->footer, third);
        add_vertical_divider(view->footer, third * 2);
    }

    lv_obj_add_event_cb(view->controls[0], adjustment_control_click, LV_EVENT_CLICKED, view);
    lv_obj_add_event_cb(view->controls[1], adjustment_control_click, LV_EVENT_CLICKED, view);
    if (view->kind == LIGHT_RGB && view->controls[2]) {
        lv_obj_add_event_cb(view->controls[2], adjustment_control_click, LV_EVENT_CLICKED, view);
    }
}

static void style_color_slider_knob(lv_obj_t *slider)
{
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_width(slider, 14, LV_PART_KNOB);
    lv_obj_set_style_height(slider, 14, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(slider, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 3, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(slider, LV_OPA_40, LV_PART_KNOB);
}

static void create_color_controls(light_view_t *view)
{
    if (!view || view->kind != LIGHT_RGB) return;

    view->hue_track = lv_obj_create(view->adjust_panel);
    lv_obj_remove_style_all(view->hue_track);
    lv_obj_set_pos(view->hue_track, 17, 27);
    lv_obj_set_size(view->hue_track, 108, 8);
    lv_obj_set_style_bg_color(view->hue_track, lv_color_hsv_to_rgb(0, 100, 100), 0);
    lv_obj_set_style_bg_opa(view->hue_track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(view->hue_track, 4, 0);
    lv_obj_set_style_clip_corner(view->hue_track, true, 0);
    lv_obj_clear_flag(view->hue_track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(view->hue_track, LV_OBJ_FLAG_SCROLLABLE);

    /* Six overlapping two-stop gradients form one continuous hue strip.
     * Overlap avoids scale/rounding seams; the parent clips only the outer corners. */
    for (int i = 0; i < 6; i++) {
        uint16_t h1 = (uint16_t)(i * 60);
        uint16_t h2 = (uint16_t)((i == 5) ? 0 : (i + 1) * 60);
        lv_obj_t *segment = rect(view->hue_track, i * 18, 0, 19, 8, 0,
                                 lv_color_hsv_to_rgb(h1, 100, 100));
        lv_obj_set_style_bg_grad_color(segment, lv_color_hsv_to_rgb(h2, 100, 100), 0);
        lv_obj_set_style_bg_grad_dir(segment, LV_GRAD_DIR_HOR, 0);
    }

    view->hue_slider = lv_slider_create(view->adjust_panel);
    lv_slider_set_range(view->hue_slider, COLOR_MIN, COLOR_MAX);
    lv_slider_set_value(view->hue_slider, view->hue_percent, LV_ANIM_OFF);
    lv_obj_set_pos(view->hue_slider, 17, 27);
    lv_obj_set_size(view->hue_slider, 108, 8);
    lv_obj_set_style_bg_opa(view->hue_slider, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view->hue_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    style_color_slider_knob(view->hue_slider);
    lv_obj_add_event_cb(view->hue_slider, hue_slider_changed, LV_EVENT_VALUE_CHANGED, view);
    lv_obj_add_event_cb(view->hue_slider, color_slider_released, LV_EVENT_RELEASED, view);

    view->saturation_slider = lv_slider_create(view->adjust_panel);
    lv_slider_set_range(view->saturation_slider, COLOR_MIN, COLOR_MAX);
    lv_slider_set_value(view->saturation_slider, view->saturation, LV_ANIM_OFF);
    lv_obj_set_pos(view->saturation_slider, 17, 72);
    lv_obj_set_size(view->saturation_slider, 108, 8);
    lv_obj_set_style_bg_opa(view->saturation_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(view->saturation_slider, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view->saturation_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    style_color_slider_knob(view->saturation_slider);
    update_saturation_gradient(view);
    lv_obj_add_event_cb(view->saturation_slider, saturation_slider_changed, LV_EVENT_VALUE_CHANGED, view);
    lv_obj_add_event_cb(view->saturation_slider, color_slider_released, LV_EVENT_RELEASED, view);

    set_color_controls_visible(view, false);
}

static void create_adjust_panel(light_view_t *view)
{
    view->adjust_panel = lv_obj_create(view->item);
    lv_obj_remove_style_all(view->adjust_panel);
    lv_obj_set_pos(view->adjust_panel, 0, 0);
    lv_obj_set_size(view->adjust_panel, ITEM_W, FOOTER_Y);
    lv_obj_set_style_bg_color(view->adjust_panel, C_CARD_ON, 0);
    lv_obj_set_style_bg_opa(view->adjust_panel, LV_OPA_COVER, 0);
    lv_obj_add_flag(view->adjust_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(view->adjust_panel, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(view->adjust_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view->adjust_panel, LV_OBJ_FLAG_HIDDEN);

    view->adjust_value = lv_label_create(view->adjust_panel);
    lv_label_set_text(view->adjust_value, "70%");
    lv_obj_set_style_text_color(view->adjust_value, C_WARM_HI, 0);
    lv_obj_set_style_text_opa(view->adjust_value, LV_OPA_COVER, 0);
    lv_obj_align(view->adjust_value, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_clear_flag(view->adjust_value, LV_OBJ_FLAG_CLICKABLE);

    view->adjust_slider = lv_slider_create(view->adjust_panel);
    lv_slider_set_range(view->adjust_slider, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    lv_slider_set_value(view->adjust_slider, view->brightness, LV_ANIM_OFF);
    lv_obj_set_size(view->adjust_slider, 108, 8);
    lv_obj_align(view->adjust_slider, LV_ALIGN_TOP_MID, 0, 63);
    lv_obj_add_event_cb(view->adjust_slider, adjust_slider_changed, LV_EVENT_VALUE_CHANGED, view);
    lv_obj_add_event_cb(view->adjust_slider, adjust_slider_released, LV_EVENT_RELEASED, view);

    create_color_controls(view);
}

static void update_state(light_view_t *view)
{
    const lv_color_t main = view->on ? C_WARM_HI : C_OFF;

    lv_obj_set_style_bg_color(view->item, view->on ? C_CARD_ON : C_CARD, 0);
#ifdef UI_LIGHTS_HAS_SOURCE_HAN
    lv_obj_set_style_text_color(view->label, view->on ? C_TEXT : C_OFF, 0);
    lv_obj_set_style_text_opa(view->label, view->on ? LV_OPA_COVER : LV_OPA_70, 0);
#else
    lv_obj_set_style_image_recolor(view->label, view->on ? C_TEXT : C_OFF, 0);
    lv_obj_set_style_image_recolor_opa(view->label, LV_OPA_COVER, 0);
#endif

    lv_obj_set_style_bg_color(view->accent, view->on ? C_WARM : C_OFF_DARK, 0);
    lv_obj_set_style_bg_opa(view->accent, view->on ? LV_OPA_COVER : LV_OPA_50, 0);
    lv_obj_set_style_shadow_color(view->accent, C_WARM, 0);
    lv_obj_set_style_shadow_width(view->accent, view->on ? 6 : 0, 0);
    lv_obj_set_style_shadow_opa(view->accent, view->on ? LV_OPA_30 : LV_OPA_TRANSP, 0);

    ui_lights_icon_set_color(view->room_icon, main);
    lv_obj_set_style_opa(view->room_icon, view->on ? LV_OPA_COVER : LV_OPA_60, 0);
    if (view->room_detail) {
        lv_obj_set_style_bg_color(view->room_detail, main, 0);
        lv_obj_set_style_bg_opa(view->room_detail, view->on ? LV_OPA_COVER : LV_OPA_60, 0);
    }

    lv_obj_set_style_bg_color(view->footer, view->on ? C_FOOTER : C_CARD, 0);
    for (int i = 0; i < 3; i++) {
        if (view->control_icons[i]) {
            ui_lights_icon_set_color(view->control_icons[i], main);
            lv_obj_set_style_opa(view->control_icons[i], view->on ? LV_OPA_COVER : LV_OPA_50, 0);
        }
    }

    clear_control_highlights(view);
    if (view->adjust_mode == ADJUST_BRIGHTNESS && view->controls[0]) {
        lv_obj_set_style_bg_color(view->controls[0], C_ADJUST_ACTIVE, 0);
        lv_obj_set_style_bg_opa(view->controls[0], LV_OPA_COVER, 0);
        ui_lights_icon_set_color(view->control_icons[0], C_WARM_HI);
        lv_obj_set_style_opa(view->control_icons[0], LV_OPA_COVER, 0);
    } else if (view->adjust_mode == ADJUST_TEMPERATURE && view->controls[1]) {
        lv_obj_set_style_bg_color(view->controls[1], C_ADJUST_ACTIVE, 0);
        lv_obj_set_style_bg_opa(view->controls[1], LV_OPA_COVER, 0);
        ui_lights_icon_set_color(view->control_icons[1], C_WARM_HI);
        lv_obj_set_style_opa(view->control_icons[1], LV_OPA_COVER, 0);
    } else if (view->adjust_mode == ADJUST_COLOR && view->controls[2]) {
        lv_obj_set_style_bg_color(view->controls[2], C_ADJUST_ACTIVE, 0);
        lv_obj_set_style_bg_opa(view->controls[2], LV_OPA_COVER, 0);
        ui_lights_icon_set_color(view->control_icons[2], selected_hue_color(view));
        lv_obj_set_style_opa(view->control_icons[2], LV_OPA_COVER, 0);
    }

    if (view->switch_dot) {
        lv_obj_t *track = lv_obj_get_parent(view->switch_dot);
        lv_obj_set_style_bg_color(track, view->on ? C_SWITCH_ON : C_OFF_DARK, 0);
        lv_obj_set_x(view->switch_dot, view->on ? 30 : 4);
        lv_obj_set_style_bg_color(view->switch_dot, main, 0);
        lv_obj_set_style_bg_opa(view->switch_dot, view->on ? LV_OPA_COVER : LV_OPA_60, 0);
    }
}

static void item_click(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    if (!view) return;
    if (view->adjust_mode != ADJUST_NONE) return;
    view->on = !view->on;
    (void)ha_lights_send(light_index(view), HA_LIGHT_POWER, view->on ? 1 : 0, 0);
    update_state(view);
    note_activity();
}

static void scroll_activity(lv_event_t *event)
{
    (void)event;
    note_activity();
}

static void create_item(lv_obj_t *parent, size_t index)
{
    const light_spec_t *spec = &SPECS[index];
    light_view_t *view = &s_views[index];
    *view = (light_view_t){0};
    view->kind = spec->kind;
    view->on = spec->initial_on;
    view->brightness = 70;
    view->color_temp_k = 4100;
    view->hue_percent = 0;
    view->saturation = 100;

    view->item = lv_obj_create(parent);
    lv_obj_remove_style_all(view->item);
    lv_obj_set_size(view->item, ITEM_W, ITEM_H);
    lv_obj_set_style_bg_color(view->item, C_CARD, 0);
    lv_obj_set_style_bg_opa(view->item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(view->item, 13, 0);
    lv_obj_clear_flag(view->item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view->item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(view->item, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(view->item, item_click, LV_EVENT_CLICKED, view);

    view->label = create_text_or_bitmap_label(view->item, spec);
    view->accent = rect(view->item, 50, 34, 42, 2, 1, C_WARM);

    view->room_icon = create_room_icon(view->item, spec, C_WARM_HI);
    lv_obj_align(view->room_icon, LV_ALIGN_TOP_MID, 0, 48);

    if (spec->extra_icon == &ui_lights_extra_tv2) {
        /* Minimal cabinet/shelf line: deliberately wider than the 40px TV glyph. */
        view->room_detail = rect(view->item, 44, 94, 54, 2, 1, C_WARM_HI);
    }

    create_adjust_panel(view);
    create_footer(view);
    update_state(view);
}

/* Runs in LVGL thread, never in the HA HTTP worker. */
static void ha_refresh_timer(lv_timer_t *timer)
{
    (void)timer;
    for (int i = 0; i < LIGHT_COUNT; ++i) {
        light_view_t *view = &s_views[i];
        if (!view->item) continue;
        ha_light_state_t state;
        if (!ha_lights_get(i, &state) || !state.available) continue;
        if (view->adjust_mode != ADJUST_NONE) continue;
        bool changed = !s_ha_initialized[i] || view->on != state.on;
        view->on = state.on;
        if (state.brightness_pct) view->brightness = state.brightness_pct;
        if (state.color_temp_k) view->color_temp_k = state.color_temp_k;
        if (view->kind == LIGHT_RGB) {
            view->hue_percent = (uint8_t)((state.hue_deg * 100U) / 360U);
            view->saturation = state.saturation_pct;
        }
        s_ha_initialized[i] = true;
        if (changed) update_state(view);
    }
}

lv_obj_t *ui_page_lights_build(lv_obj_t *parent,
                               ui_lights_activity_cb_t activity_cb,
                               void *activity_user_data)
{
    /* Normal routing always stops the old page first. Keep build idempotent anyway,
       so an accidental second build cannot leak an LVGL tree or a 30 s timer. */
    if (s_root) ui_page_lights_stop();

    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_adjust_view = NULL;
    stop_adjust_timer();
    for (int i = 0; i < LIGHT_COUNT; ++i) s_ha_initialized[i] = false;
    ui_lights_extra_icons_init();
#ifndef UI_LIGHTS_HAS_SOURCE_HAN
    if (!s_labels_ready) {
        ui_lights_labels_init();
        s_labels_ready = true;
    }
#endif

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(s_root, C_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *scroller = lv_obj_create(s_root);
    lv_obj_remove_style_all(scroller);
    lv_obj_set_size(scroller, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scroller, C_BG, 0);
    lv_obj_set_style_bg_opa(scroller, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(scroller, LEFT_PAD, 0);
    lv_obj_set_style_pad_right(scroller, RIGHT_PAD, 0);
    lv_obj_set_style_pad_column(scroller, GAP, 0);
    lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scroller, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(scroller, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_event_cb(scroller, scroll_activity, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(scroller, scroll_activity, LV_EVENT_SCROLL, NULL);

    for (size_t i = 0; i < LIGHT_COUNT; i++) create_item(scroller, i);
    s_ha_timer = lv_timer_create(ha_refresh_timer, 1000, NULL);
    ha_refresh_timer(NULL);
    return s_root;
}

void ui_page_lights_stop(void)
{
    if (s_ha_timer) { lv_timer_delete(s_ha_timer); s_ha_timer = NULL; }
    stop_adjust_timer();
    s_adjust_view = NULL;

    lv_obj_t *root = s_root;
    s_root = NULL;
    if (root) lv_obj_delete(root);

    for (size_t i = 0; i < LIGHT_COUNT; i++) s_views[i] = (light_view_t){0};
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}
