#include "ui_page_settings.h"
#include "ui_system_icons.h"

#include <stdint.h>

#define UI_SCREEN_W               640
#define UI_SCREEN_H               172
#define UI_BACK_RAIL_W             56
#define UI_CONTENT_X               UI_BACK_RAIL_W
#define UI_CONTENT_W              (UI_SCREEN_W - UI_BACK_RAIL_W)
#define UI_CONTENT_H              124
#define UI_NAV_Y                  124
#define UI_TAB_COUNT                6
#define UI_TAB_W                   97
#define UI_TAB_LAST_W              99
#define UI_TAB_H                   48

#define UI_SLIDER_W               420
#define UI_SLIDER_H                 8
#define UI_SLIDER_X              (UI_CONTENT_X + 108)
#define UI_SLIDER_Y                58
#define UI_CONTENT_ICON_X          86
#define UI_CONTENT_ICON_Y          46
#define UI_VALUE_Y                 22

#define UI_STATUS_LEFT_W          154
#define UI_STATUS_RIGHT_X         UI_STATUS_LEFT_W
#define UI_STATUS_RIGHT_W        (UI_CONTENT_W - UI_STATUS_LEFT_W)

#define UI_COLOR_BG          lv_color_hex(0x000000)
#define UI_COLOR_ACCENT      lv_color_hex(0x45D7F0)
#define UI_COLOR_FG          lv_color_hex(0xF4F7F8)
#define UI_COLOR_MUTED       lv_color_hex(0x6B7178)
#define UI_COLOR_TRACK       lv_color_hex(0x2A2D31)
#define UI_COLOR_TAB         lv_color_hex(0x0C0E11)
#define UI_COLOR_TAB_ACTIVE  lv_color_hex(0x20262D)
#define UI_COLOR_PANEL       lv_color_hex(0x0D1014)
#define UI_COLOR_PANEL_2     lv_color_hex(0x151A20)
#define UI_COLOR_DANGER      lv_color_hex(0xE05A63)

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

/* UI-only state for now. Networking/AI/BLE modules inject real state later. */
static bool s_wifi_enabled = true;
static bool s_wifi_configured = false;
static bool s_wifi_connected = false;
static bool s_ai_configured = false;
static bool s_ai_online = false;

static bool s_bt_enabled = true;
static bool s_bt_scanning = false;
static int32_t s_bt_selected_paired = -1;
static int32_t s_bt_pairing_available = -1;
static bool s_bt_paired_visible[2] = { true, true };

static const char *s_bt_paired_names[2] = {
    "Desk Knob",
    "Temp Sensor",
};

static const char *s_bt_available_names[2] = {
    "BLE Remote",
    "Room Sensor",
};

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

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    return label;
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
    int32_t x = (UI_SLIDER_X - UI_CONTENT_X) + (value * UI_SLIDER_W) / 100 - label_w / 2;
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
    if (code != LV_EVENT_VALUE_CHANGED && code != LV_EVENT_PRESSED &&
        code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED &&
        code != LV_EVENT_PRESS_LOST) return;

    note_activity();
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        update_value_label();
        set_value_label_visible(true);
    }

    if (code == LV_EVENT_VALUE_CHANGED && !s_slider_ctx.adjusting) {
        int32_t raw = lv_slider_get_value(s_slider_ctx.slider);
        int32_t snapped = snap_value(raw, s_slider_ctx.actual_min,
                                     s_slider_ctx.max_value, s_slider_ctx.step);
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

static void add_status_divider(lv_obj_t *parent)
{
    lv_obj_t *line = plain_obj(parent);
    lv_obj_set_size(line, 1, 94);
    lv_obj_set_pos(line, UI_STATUS_LEFT_W, 15);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x22262B), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
}

static lv_obj_t *add_action_button(lv_obj_t *parent, const char *text,
                                   int32_t x, int32_t y, int32_t w,
                                   lv_color_t bg, lv_color_t fg)
{
    lv_obj_t *btn = plain_obj(parent);
    lv_obj_set_size(btn, w, 25);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *label = make_label(btn, text, fg);
    lv_obj_center(label);
    return btn;
}

/* Placeholder QR visual. The real payload/URL is added with the Web setup service. */
static void build_qr_placeholder(lv_obj_t *parent, int32_t x, int32_t y)
{
    const int32_t size = 82;
    const int32_t cells = 13;
    const int32_t cell = 6;

    lv_obj_t *plate = plain_obj(parent);
    lv_obj_set_size(plate, size, size);
    lv_obj_set_pos(plate, x, y);
    lv_obj_set_style_bg_color(plate, UI_COLOR_FG, 0);
    lv_obj_set_style_bg_opa(plate, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(plate, 4, 0);

    for (int32_t r = 0; r < cells; ++r) {
        for (int32_t c = 0; c < cells; ++c) {
            bool finder_tl = (r < 5 && c < 5 &&
                             (r == 0 || r == 4 || c == 0 || c == 4 ||
                              (r == 2 && c == 2)));
            bool finder_tr = (r < 5 && c >= 8 &&
                             (r == 0 || r == 4 || c == 8 || c == 12 ||
                              (r == 2 && c == 10)));
            bool finder_bl = (r >= 8 && c < 5 &&
                             (r == 8 || r == 12 || c == 0 || c == 4 ||
                              (r == 10 && c == 2)));
            bool data = (((r * 7 + c * 11 + r * c) % 5) < 2);
            if (!(finder_tl || finder_tr || finder_bl || data)) continue;

            lv_obj_t *dot = plain_obj(plate);
            lv_obj_set_size(dot, cell, cell);
            lv_obj_set_pos(dot, 2 + c * cell, 2 + r * cell);
            lv_obj_set_style_bg_color(dot, UI_COLOR_BG, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        }
    }
}

static void wifi_toggle_event_cb(lv_event_t *e);
static void bt_toggle_event_cb(lv_event_t *e);
static void bt_scan_event_cb(lv_event_t *e);
static void bt_paired_event_cb(lv_event_t *e);
static void bt_forget_event_cb(lv_event_t *e);
static void bt_available_event_cb(lv_event_t *e);
static void show_tab(ui_settings_tab_t tab);

static void build_wifi_left(lv_obj_t *parent)
{
    lv_obj_t *icon = ui_system_icon_wifi(parent,
                                         s_wifi_enabled ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, -UI_STATUS_RIGHT_W / 2, 20);

    lv_obj_t *toggle = plain_obj(parent);
    lv_obj_set_size(toggle, 64, 28);
    lv_obj_set_pos(toggle, 45, 73);
    lv_obj_set_style_bg_color(toggle,
                              s_wifi_enabled ? UI_COLOR_ACCENT : UI_COLOR_PANEL_2, 0);
    lv_obj_set_style_bg_opa(toggle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(toggle, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(toggle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(toggle, wifi_toggle_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *knob = plain_obj(toggle);
    lv_obj_set_size(knob, 22, 22);
    lv_obj_set_pos(knob, s_wifi_enabled ? 39 : 3, 3);
    lv_obj_set_style_bg_color(knob, UI_COLOR_FG, 0);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(knob, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *state = make_label(parent, s_wifi_enabled ? "ON" : "OFF",
                                 s_wifi_enabled ? UI_COLOR_FG : UI_COLOR_MUTED);
    lv_obj_set_pos(state, 68, 102);
}

static void build_wifi_right(lv_obj_t *parent)
{
    const int32_t rx = UI_STATUS_RIGHT_X + 16;

    if (!s_wifi_enabled) {
        lv_obj_t *label = make_label(parent, "WI-FI DISABLED", UI_COLOR_MUTED);
        lv_obj_align(label, LV_ALIGN_CENTER, 80, 0);
        return;
    }

    if (!s_wifi_configured || !s_wifi_connected) {
        build_qr_placeholder(parent, UI_STATUS_RIGHT_X + 36, 20);
        lv_obj_t *title = make_label(parent,
                                     s_wifi_configured ? "NO CONNECTION" : "SCAN TO CONFIGURE",
                                     UI_COLOR_MUTED);
        lv_obj_set_pos(title, UI_STATUS_RIGHT_X + 138, 43);
        lv_obj_t *hint = make_label(parent, "PHONE SETUP", UI_COLOR_ACCENT);
        lv_obj_set_pos(hint, UI_STATUS_RIGHT_X + 138, 68);
        return;
    }

    lv_obj_t *ssid = make_label(parent, "ChenHome_5G", UI_COLOR_FG);
    lv_obj_set_pos(ssid, rx, 8);
    lv_obj_t *connected = make_label(parent, "CONNECTED", UI_COLOR_ACCENT);
    lv_obj_align(connected, LV_ALIGN_TOP_RIGHT, -12, 8);

    const char *names[] = {"IP", "MAC", "DNS"};
    const char *values[] = {
        "192.168.50.88",
        "AA:BB:CC:DD:EE:FF",
        "192.168.50.2",
    };
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *name = make_label(parent, names[i], UI_COLOR_MUTED);
        lv_obj_set_pos(name, rx, 33 + i * 22);
        lv_obj_t *value = make_label(parent, values[i], UI_COLOR_FG);
        lv_obj_set_pos(value, rx + 46, 33 + i * 22);
    }

    add_action_button(parent, "DISCONNECT", rx + 238, 36, 112,
                      UI_COLOR_PANEL_2, UI_COLOR_FG);
    add_action_button(parent, "FORGET", rx + 238, 70, 112,
                      lv_color_hex(0x2A171A), UI_COLOR_DANGER);
}

static void build_wifi_content(void)
{
    build_wifi_left(s_content);
    add_status_divider(s_content);
    build_wifi_right(s_content);
}

static void wifi_toggle_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_wifi_enabled = !s_wifi_enabled;
    if (!s_wifi_enabled) s_wifi_connected = false;
    note_activity();
    show_tab(UI_SETTINGS_WIFI);
}

static void build_bluetooth_left(lv_obj_t *parent)
{
    lv_obj_t *icon = ui_system_icon_bluetooth(parent,
                                              s_bt_enabled ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(icon, 61, 14);

    lv_obj_t *toggle = plain_obj(parent);
    lv_obj_set_size(toggle, 64, 28);
    lv_obj_set_pos(toggle, 45, 50);
    lv_obj_set_style_bg_color(toggle,
                              s_bt_enabled ? UI_COLOR_ACCENT : UI_COLOR_PANEL_2, 0);
    lv_obj_set_style_bg_opa(toggle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(toggle, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(toggle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(toggle, bt_toggle_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *knob = plain_obj(toggle);
    lv_obj_set_size(knob, 22, 22);
    lv_obj_set_pos(knob, s_bt_enabled ? 39 : 3, 3);
    lv_obj_set_style_bg_color(knob, UI_COLOR_FG, 0);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(knob, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *state = make_label(parent, s_bt_enabled ? "ON" : "OFF",
                                 s_bt_enabled ? UI_COLOR_FG : UI_COLOR_MUTED);
    lv_obj_set_pos(state, 68, 79);

    lv_obj_t *scan = add_action_button(parent,
                                       s_bt_scanning ? "SCANNING..." : "SCAN",
                                       39, 97, 76,
                                       s_bt_enabled ? UI_COLOR_PANEL_2 : UI_COLOR_PANEL,
                                       s_bt_enabled ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    if (s_bt_enabled) lv_obj_add_event_cb(scan, bt_scan_event_cb, LV_EVENT_CLICKED, NULL);
}

static lv_obj_t *build_bt_row(lv_obj_t *parent, const char *name,
                              int32_t x, int32_t y, int32_t w,
                              bool active)
{
    lv_obj_t *row = plain_obj(parent);
    lv_obj_set_size(row, w, 22);
    lv_obj_set_pos(row, x, y);
    lv_obj_set_style_bg_color(row, active ? UI_COLOR_PANEL_2 : UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 7, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = make_label(row, name, active ? UI_COLOR_FG : UI_COLOR_MUTED);
    lv_obj_set_pos(label, 8, 3);
    return row;
}

static void build_bluetooth_right(lv_obj_t *parent)
{
    const int32_t x = UI_STATUS_RIGHT_X + 12;
    const int32_t w = UI_STATUS_RIGHT_W - 24;

    if (!s_bt_enabled) {
        lv_obj_t *label = make_label(parent, "BLUETOOTH DISABLED", UI_COLOR_MUTED);
        lv_obj_align(label, LV_ALIGN_CENTER, 80, 0);
        return;
    }

    lv_obj_t *paired_title = make_label(parent, "PAIRED", UI_COLOR_MUTED);
    lv_obj_set_pos(paired_title, x, 4);

    int32_t row_y = 23;
    for (int i = 0; i < 2; ++i) {
        if (!s_bt_paired_visible[i]) continue;
        bool active = (s_bt_selected_paired == i);
        lv_obj_t *row = build_bt_row(parent, s_bt_paired_names[i], x, row_y, w, active);
        lv_obj_add_event_cb(row, bt_paired_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        lv_obj_t *status = make_label(row,
                                      (i == 0) ? "CONNECTED" : "SAVED",
                                      (i == 0) ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
        lv_obj_align(status, LV_ALIGN_RIGHT_MID, active ? -78 : -8, 0);

        if (active) {
            lv_obj_t *forget = add_action_button(row, "FORGET",
                                                 w - 72, -1, 68,
                                                 lv_color_hex(0x2A171A), UI_COLOR_DANGER);
            lv_obj_add_event_cb(forget, bt_forget_event_cb, LV_EVENT_CLICKED,
                                (void *)(uintptr_t)i);
        }
        row_y += 25;
    }

    lv_obj_t *sep = plain_obj(parent);
    lv_obj_set_size(sep, w, 1);
    lv_obj_set_pos(sep, x, 72);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x22262B), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    lv_obj_t *available_title = make_label(parent,
                                           s_bt_scanning ? "AVAILABLE · SCANNING" : "AVAILABLE",
                                           s_bt_scanning ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(available_title, x, 78);

    for (int i = 0; i < 2; ++i) {
        bool pairing = (s_bt_pairing_available == i);
        lv_obj_t *row = build_bt_row(parent, s_bt_available_names[i],
                                     x, 96 + i * 25, w, pairing);
        lv_obj_add_event_cb(row, bt_available_event_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
        lv_obj_t *status = make_label(row,
                                      pairing ? "PAIRING..." : ((i == 0) ? "-58 dBm" : "-72 dBm"),
                                      pairing ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
        lv_obj_align(status, LV_ALIGN_RIGHT_MID, -8, 0);
    }
}

static void build_bluetooth_content(void)
{
    build_bluetooth_left(s_content);
    add_status_divider(s_content);
    build_bluetooth_right(s_content);
}

static void bt_toggle_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_bt_enabled = !s_bt_enabled;
    if (!s_bt_enabled) {
        s_bt_scanning = false;
        s_bt_selected_paired = -1;
        s_bt_pairing_available = -1;
    }
    note_activity();
    show_tab(UI_SETTINGS_BLUETOOTH);
}

static void bt_scan_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_bt_enabled) return;
    s_bt_scanning = !s_bt_scanning;
    s_bt_pairing_available = -1;
    note_activity();
    show_tab(UI_SETTINGS_BLUETOOTH);
}

static void bt_paired_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int32_t index = (int32_t)(uintptr_t)lv_event_get_user_data(e);
    s_bt_selected_paired = (s_bt_selected_paired == index) ? -1 : index;
    note_activity();
    show_tab(UI_SETTINGS_BLUETOOTH);
}

static void bt_forget_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int32_t index = (int32_t)(uintptr_t)lv_event_get_user_data(e);
    if (index >= 0 && index < 2) s_bt_paired_visible[index] = false;
    s_bt_selected_paired = -1;
    note_activity();
    show_tab(UI_SETTINGS_BLUETOOTH);
}

static void bt_available_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_bt_enabled) return;
    int32_t index = (int32_t)(uintptr_t)lv_event_get_user_data(e);
    s_bt_pairing_available = index;
    s_bt_scanning = false;
    note_activity();
    show_tab(UI_SETTINGS_BLUETOOTH);
}

static void build_ai_content(void)
{
    lv_obj_t *icon = ui_system_icon_ai_robot2(s_content,
                                               s_ai_online ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(icon, 61, 24);

    lv_obj_t *state = make_label(s_content,
                                 s_ai_configured ? (s_ai_online ? "ONLINE" : "OFFLINE") : "NOT SET",
                                 s_ai_online ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(state, 52, 79);

    add_status_divider(s_content);

    if (!s_ai_configured) {
        build_qr_placeholder(s_content, UI_STATUS_RIGHT_X + 36, 20);
        lv_obj_t *title = make_label(s_content, "SCAN TO CONFIGURE", UI_COLOR_MUTED);
        lv_obj_set_pos(title, UI_STATUS_RIGHT_X + 138, 43);
        lv_obj_t *hint = make_label(s_content, "AI SERVER SETUP", UI_COLOR_ACCENT);
        lv_obj_set_pos(hint, UI_STATUS_RIGHT_X + 138, 68);
        return;
    }

    const int32_t x = UI_STATUS_RIGHT_X + 18;
    const char *names[] = {"SERVER", "PORT", "MODEL", "STATUS"};
    const char *values[] = {
        "192.168.50.149",
        "8000",
        "qwen3.5:9b",
        s_ai_online ? "ONLINE" : "OFFLINE",
    };
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *name = make_label(s_content, names[i], UI_COLOR_MUTED);
        lv_obj_set_pos(name, x, 12 + i * 24);
        lv_obj_t *value = make_label(s_content, values[i],
                                     (i == 3 && s_ai_online) ? UI_COLOR_ACCENT : UI_COLOR_FG);
        lv_obj_set_pos(value, x + 72, 12 + i * 24);
    }
}

static void refresh_tab_styles(void)
{
    for (int i = 0; i < UI_TAB_COUNT; ++i) {
        bool active = ((ui_settings_tab_t)i == s_selected);
        lv_obj_set_style_bg_color(s_tab_cards[i],
                                  active ? UI_COLOR_TAB_ACTIVE : UI_COLOR_TAB, 0);
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
    else if (tab == UI_SETTINGS_WIFI) {
        build_wifi_content();
    }
    else if (tab == UI_SETTINGS_BLUETOOTH) {
        build_bluetooth_content();
    }
    else if (tab == UI_SETTINGS_AI) {
        build_ai_content();
    }
    /* System remains a placeholder for now. */

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
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, tab_event_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);

        lv_obj_t *icon = make_icon(card, (ui_settings_tab_t)i, UI_COLOR_MUTED);
        s_tab_icons[i] = icon;
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 8);
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
