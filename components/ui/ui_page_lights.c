#include "ui_page_lights.h"
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
#define C_SWITCH_ON     lv_color_hex(0x3A301F)
#define C_ADJUST_ACTIVE lv_color_hex(0x2A2317)

typedef enum { LIGHT_NORMAL=0, LIGHT_RGB, LIGHT_SWITCH_ONLY } light_kind_t;

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
    lv_obj_t *brightness_slider;
    lv_obj_t *brightness_value;
    light_kind_t kind;
    bool on;
    bool brightness_adjusting;
    uint8_t brightness;
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

static lv_obj_t *s_root;
static ui_lights_activity_cb_t s_activity_cb;
static void *s_activity_user_data;
static light_view_t s_views[8];
static bool s_labels_ready;
static light_view_t *s_adjust_view;
static lv_timer_t *s_adjust_timer;

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

static void update_brightness_value(light_view_t *view)
{
    if (!view || !view->brightness_value) return;
    lv_label_set_text_fmt(view->brightness_value, "%u%%", (unsigned)view->brightness);
}

static void stop_adjust_timer(void)
{
    if (!s_adjust_timer) return;
    lv_timer_delete(s_adjust_timer);
    s_adjust_timer = NULL;
}

static void exit_brightness_adjust(light_view_t *view)
{
    if (!view || !view->brightness_adjusting) return;

    view->brightness_adjusting = false;
    lv_obj_add_flag(view->adjust_panel, LV_OBJ_FLAG_HIDDEN);
    set_default_content_visible(view, true);

    if (view->controls[0]) {
        lv_obj_set_style_bg_opa(view->controls[0], LV_OPA_TRANSP, 0);
    }

    if (s_adjust_view == view) s_adjust_view = NULL;
    stop_adjust_timer();
}

static void adjust_timeout_cb(lv_timer_t *timer)
{
    light_view_t *view = (light_view_t *)lv_timer_get_user_data(timer);
    s_adjust_timer = NULL;
    if (view) exit_brightness_adjust(view);
}

static void restart_adjust_timer(light_view_t *view)
{
    stop_adjust_timer();
    s_adjust_timer = lv_timer_create(adjust_timeout_cb, ADJUST_TIMEOUT_MS, view);
    lv_timer_set_repeat_count(s_adjust_timer, 1);
}

static void enter_brightness_adjust(light_view_t *view)
{
    if (!view || !view->controls[0]) return;

    if (s_adjust_view && s_adjust_view != view) exit_brightness_adjust(s_adjust_view);

    view->brightness_adjusting = true;
    s_adjust_view = view;
    set_default_content_visible(view, false);
    lv_obj_clear_flag(view->adjust_panel, LV_OBJ_FLAG_HIDDEN);
    lv_slider_set_value(view->brightness_slider, view->brightness, LV_ANIM_OFF);
    update_brightness_value(view);

    lv_obj_set_style_bg_color(view->controls[0], C_ADJUST_ACTIVE, 0);
    lv_obj_set_style_bg_opa(view->controls[0], LV_OPA_COVER, 0);

    restart_adjust_timer(view);
    note_activity();
}

static void brightness_control_click(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    if (!view) return;

    if (view->brightness_adjusting) exit_brightness_adjust(view);
    else enter_brightness_adjust(view);

    note_activity();
}

static void brightness_slider_changed(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    lv_obj_t *slider = lv_event_get_target_obj(event);
    if (!view || !slider) return;

    int32_t value = lv_slider_get_value(slider);
    if (value < 1) value = 1;
    if (value > 100) value = 100;
    view->brightness = (uint8_t)value;
    update_brightness_value(view);
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

    lv_obj_add_event_cb(view->controls[0], brightness_control_click, LV_EVENT_CLICKED, view);
}

static void create_brightness_adjust_panel(light_view_t *view)
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

    view->brightness_value = lv_label_create(view->adjust_panel);
    lv_label_set_text(view->brightness_value, "70%");
    lv_obj_set_style_text_color(view->brightness_value, C_WARM_HI, 0);
    lv_obj_set_style_text_opa(view->brightness_value, LV_OPA_COVER, 0);
    lv_obj_align(view->brightness_value, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_clear_flag(view->brightness_value, LV_OBJ_FLAG_CLICKABLE);

    view->brightness_slider = lv_slider_create(view->adjust_panel);
    lv_slider_set_range(view->brightness_slider, 1, 100);
    lv_slider_set_value(view->brightness_slider, view->brightness, LV_ANIM_OFF);
    lv_obj_set_size(view->brightness_slider, 108, 8);
    lv_obj_align(view->brightness_slider, LV_ALIGN_TOP_MID, 0, 63);
    lv_obj_set_style_bg_color(view->brightness_slider, C_OFF_DARK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view->brightness_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(view->brightness_slider, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(view->brightness_slider, C_WARM, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(view->brightness_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(view->brightness_slider, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(view->brightness_slider, C_WARM_HI, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(view->brightness_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_width(view->brightness_slider, 20, LV_PART_KNOB);
    lv_obj_set_style_height(view->brightness_slider, 20, LV_PART_KNOB);
    lv_obj_set_style_radius(view->brightness_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(view->brightness_slider, C_WARM, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(view->brightness_slider, 6, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(view->brightness_slider, LV_OPA_30, LV_PART_KNOB);
    lv_obj_add_event_cb(view->brightness_slider, brightness_slider_changed, LV_EVENT_VALUE_CHANGED, view);
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

    if (view->brightness_adjusting && view->controls[0]) {
        lv_obj_set_style_bg_color(view->controls[0], C_ADJUST_ACTIVE, 0);
        lv_obj_set_style_bg_opa(view->controls[0], LV_OPA_COVER, 0);
        ui_lights_icon_set_color(view->control_icons[0], C_WARM_HI);
        lv_obj_set_style_opa(view->control_icons[0], LV_OPA_COVER, 0);
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
    if (view->brightness_adjusting) return;
    view->on = !view->on;
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

    create_brightness_adjust_panel(view);
    create_footer(view);
    update_state(view);
}

lv_obj_t *ui_page_lights_build(lv_obj_t *parent,
                               ui_lights_activity_cb_t activity_cb,
                               void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_adjust_view = NULL;
    stop_adjust_timer();
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

    for (size_t i = 0; i < 8; i++) create_item(scroller, i);
    return s_root;
}

void ui_page_lights_stop(void)
{
    stop_adjust_timer();
    s_adjust_view = NULL;
    if (s_root) lv_obj_delete(s_root);
    s_root = NULL;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}
