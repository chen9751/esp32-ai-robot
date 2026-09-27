#include "ui_page_settings.h"
#include "ui_system_icons.h"

#include <stdint.h>

#define UI_SCREEN_W               640
#define UI_SCREEN_H               172
#define UI_BACK_RAIL_W             56
#define UI_CONTENT_X               UI_BACK_RAIL_W
#define UI_CONTENT_W              (UI_SCREEN_W - UI_BACK_RAIL_W)
#define UI_CONTENT_H              120
#define UI_NAV_Y                  122
#define UI_TAB_COUNT                6
#define UI_TAB_W                   97
#define UI_TAB_LAST_W              99
#define UI_TAB_H                   64
#define UI_SLIDER_W               420
#define UI_SLIDER_H                 8
#define UI_SLIDER_X              (UI_CONTENT_X + 108)
#define UI_SLIDER_Y                54
#define UI_CONTENT_ICON_X          86
#define UI_CONTENT_ICON_Y          42
#define UI_VALUE_Y                 18

#define UI_COLOR_BG          lv_color_hex(0x000000)
#define UI_COLOR_ACCENT      lv_color_hex(0x45D7F0)
#define UI_COLOR_FG          lv_color_hex(0xF4F7F8)
#define UI_COLOR_MUTED       lv_color_hex(0x6E747B)
#define UI_COLOR_TRACK       lv_color_hex(0x2A2D31)
#define UI_COLOR_TAB         lv_color_hex(0x0B0D10)
#define UI_COLOR_TAB_ACTIVE  lv_color_hex(0x323B46)

typedef enum {
    UI_SETTINGS_SOUND = 0,
    UI_SETTINGS_DISPLAY,
    UI_SETTINGS_WIFI,
    UI_SETTINGS_BLUETOOTH,
    UI_SETTINGS_AI,
    UI_SETTINGS_SYSTEM,
} ui_settings_tab_t;

typedef struct {
    lv_obj_t *slider;
    lv_obj_t *value_label;
    ui_settings_tab_t tab;
    int32_t actual_min;
    int32_t max_value;
    int32_t step;
    bool adjusting;
} slider_ctx_t;

static ui_settings_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static lv_obj_t *s_root = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_tab_cards[UI_TAB_COUNT] = {0};
static lv_obj_t *s_tab_icons[UI_TAB_COUNT] = {0};
static ui_settings_tab_t s_selected = UI_SETTINGS_SOUND;
static slider_ctx_t s_slider_ctx = {0};
static int32_t s_sound_value = 70;
static int32_t s_brightness_value = 60;

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

static lv_obj_t *make_icon(lv_obj_t *parent, ui_settings_tab_t tab, lv_color_t color)
{
    switch (tab) {
        case UI_SETTINGS_SOUND:     return ui_system_icon_volume(parent, color);
        case UI_SETTINGS_DISPLAY:   return ui_system_icon_brightness(parent, color);
        case UI_SETTINGS_WIFI:      return ui_system_icon_wifi(parent, color);
        case UI_SETTINGS_BLUETOOTH: return ui_system_icon_bluetooth(parent, color);
        case UI_SETTINGS_AI:        return ui_system_icon_ai_robot2(parent, color);
        case UI_SETTINGS_SYSTEM:
        default:                    return ui_system_icon_system(parent, color);
    }
}

static void style_slider(lv_obj_t *slider)
{
    lv_obj_set_size(slider, UI_SLIDER_W, UI_SLIDER_H);

    lv_obj_set_style_bg_color(slider, UI_COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    lv_obj_set_style_bg_color(slider, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(slider, UI_COLOR_FG, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_width(slider, 18, LV_PART_KNOB);
    lv_obj_set_style_height(slider, 18, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
}

static int32_t snap_value(int32_t raw, int32_t actual_min, int32_t max_value, int32_t step)
{
    if (raw < actual_min) raw = actual_min;
    if (raw > max_value) raw = max_value;

    int32_t relative = raw - actual_min;
    int32_t snapped = actual_min + ((relative + step / 2) / step) * step;
    if (snapped < actual_min) snapped = actual_min;
    if (snapped > max_value) snapped = max_value;
    return snapped;
}

static void update_value_label(void)
{
    if (s_slider_ctx.slider == NULL || s_slider_ctx.value_label == NULL) return;

    int32_t value = lv_slider_get_value(s_slider_ctx.slider);
    lv_label_set_text_fmt(s_slider_ctx.value_label, "%ld%%", (long)value);
    lv_obj_update_layout(s_slider_ctx.value_label);

    int32_t label_w = lv_obj_get_width(s_slider_ctx.value_label);
    int32_t x = (UI_SLIDER_X - UI_CONTENT_X)
              + (value * UI_SLIDER_W) / 100
              - label_w / 2;
    if (x < 0) x = 0;
    if (x + label_w > UI_CONTENT_W) x = UI_CONTENT_W - label_w;

    lv_obj_set_pos(s_slider_ctx.value_label, x, UI_VALUE_Y);
}

static void set_value_label_visible(bool visible)
{
    if (s_slider_ctx.value_label == NULL) return;
    if (visible) lv_obj_clear_flag(s_slider_ctx.value_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_slider_ctx.value_label, LV_OBJ_FLAG_HIDDEN);
}

static void slider_event_cb(lv_event_t *e)
{
    if (s_slider_ctx.slider == NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED &&
        code != LV_EVENT_PRESSED &&
        code != LV_EVENT_PRESSING &&
        code != LV_EVENT_RELEASED &&
        code != LV_EVENT_PRESS_LOST) {
        return;
    }

    note_activity();

    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        update_value_label();
        set_value_label_visible(true);
    }

    if (code == LV_EVENT_VALUE_CHANGED && !s_slider_ctx.adjusting) {
        int32_t raw = lv_slider_get_value(s_slider_ctx.slider);
        int32_t snapped = snap_value(raw,
                                     s_slider_ctx.actual_min,
                                     s_slider_ctx.max_value,
                                     s_slider_ctx.step);
        if (raw != snapped) {
            s_slider_ctx.adjusting = true;
            lv_slider_set_value(s_slider_ctx.slider, snapped, LV_ANIM_OFF);
            s_slider_ctx.adjusting = false;
        }

        int32_t value = lv_slider_get_value(s_slider_ctx.slider);
        if (s_slider_ctx.tab == UI_SETTINGS_SOUND) s_sound_value = value;
        else if (s_slider_ctx.tab == UI_SETTINGS_DISPLAY) s_brightness_value = value;

        update_value_label();
        set_value_label_visible(true);
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        set_value_label_visible(false);
    }
}

static void build_slider_content(ui_settings_tab_t tab)
{
    lv_obj_t *icon = make_icon(s_content, tab, UI_COLOR_FG);
    lv_obj_set_pos(icon, UI_CONTENT_ICON_X - UI_CONTENT_X, UI_CONTENT_ICON_Y);

    lv_obj_t *value_label = lv_label_create(s_content);
    lv_obj_set_style_text_color(value_label, UI_COLOR_FG, 0);
    lv_obj_set_style_text_opa(value_label, LV_OPA_COVER, 0);
    lv_obj_add_flag(value_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *slider = lv_slider_create(s_content);
    style_slider(slider);
    lv_obj_set_pos(slider, UI_SLIDER_X - UI_CONTENT_X, UI_SLIDER_Y);

    s_slider_ctx.slider = slider;
    s_slider_ctx.value_label = value_label;
    s_slider_ctx.tab = tab;
    s_slider_ctx.adjusting = false;

    if (tab == UI_SETTINGS_SOUND) {
        s_slider_ctx.actual_min = 0;
        s_slider_ctx.max_value = 100;
        s_slider_ctx.step = 5;
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, s_sound_value, LV_ANIM_OFF);
    }
    else {
        /* The visual track is 0..100, but brightness is clamped to 10..100. */
        s_slider_ctx.actual_min = 10;
        s_slider_ctx.max_value = 100;
        s_slider_ctx.step = 10;
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, s_brightness_value, LV_ANIM_OFF);
    }

    update_value_label();

    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_PRESS_LOST, NULL);
}

static void refresh_tab_styles(void)
{
    for (int i = 0; i < UI_TAB_COUNT; ++i) {
        bool active = ((ui_settings_tab_t)i == s_selected);
        lv_obj_set_style_bg_color(s_tab_cards[i],
                                  active ? UI_COLOR_TAB_ACTIVE : UI_COLOR_TAB,
                                  0);
        lv_obj_set_style_bg_opa(s_tab_cards[i], LV_OPA_COVER, 0);
        ui_system_icon_set_color(s_tab_icons[i],
                                 active ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    }
}

static void show_tab(ui_settings_tab_t tab)
{
    if (s_content == NULL) return;

    s_selected = tab;
    lv_obj_clean(s_content);
    s_slider_ctx = (slider_ctx_t){0};

    if (tab == UI_SETTINGS_SOUND || tab == UI_SETTINGS_DISPLAY) {
        build_slider_content(tab);
    }
    /* Wi-Fi / Bluetooth / AI / System intentionally remain blank for now. */

    refresh_tab_styles();
    note_activity();
}

static void tab_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_settings_tab_t tab = (ui_settings_tab_t)(uintptr_t)lv_event_get_user_data(e);
    show_tab(tab);
}

static void build_nav(void)
{
    int32_t x = UI_CONTENT_X;

    for (int i = 0; i < UI_TAB_COUNT; ++i) {
        int32_t width = (i == UI_TAB_COUNT - 1) ? UI_TAB_LAST_W : UI_TAB_W;

        lv_obj_t *card = plain_obj(s_root);
        s_tab_cards[i] = card;
        lv_obj_set_size(card, width, UI_TAB_H);
        lv_obj_set_pos(card, x, UI_NAV_Y);
        /* The cards extend below the 172 px viewport. The clipped lower
         * corners therefore appear square while only the top corners remain
         * rounded. Adjacent cards touch with no horizontal gap. */
        lv_obj_set_style_radius(card, 14, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card,
                            tab_event_cb,
                            LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);

        lv_obj_t *icon = make_icon(card,
                                   (ui_settings_tab_t)i,
                                   UI_COLOR_MUTED);
        s_tab_icons[i] = icon;
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 13);

        x += width;
    }
}

lv_obj_t *ui_page_settings_build(lv_obj_t *parent,
                                 ui_settings_activity_cb_t activity_cb,
                                 void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_selected = UI_SETTINGS_SOUND;

    s_root = plain_obj(parent);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    s_content = plain_obj(s_root);
    lv_obj_set_size(s_content, UI_CONTENT_W, UI_CONTENT_H);
    lv_obj_set_pos(s_content, UI_CONTENT_X, 0);

    build_nav();
    show_tab(UI_SETTINGS_SOUND);
    return s_root;
}

void ui_page_settings_stop(void)
{
    s_root = NULL;
    s_content = NULL;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
    s_selected = UI_SETTINGS_SOUND;
    s_slider_ctx = (slider_ctx_t){0};

    for (int i = 0; i < UI_TAB_COUNT; ++i) {
        s_tab_cards[i] = NULL;
        s_tab_icons[i] = NULL;
    }
}
