#include "ui_page_settings.h"
#include "board.h"
#include "ui_system_icons.h"
#include <stdbool.h>
#if defined(ESP_PLATFORM)
#include "network_service.h"
#else
typedef struct {
    bool initialized;
    bool enabled;
    bool configured;
    bool connected;
    bool time_synced;
    char ssid[33];
    char ip[16];
    char mac[18];
    char dns[16];
} network_wifi_status_t;
#endif

#include <stdint.h>
#include <string.h>

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

typedef enum {
    EDIT_NONE = 0,
    EDIT_WIFI_SSID,
    EDIT_WIFI_PASSWORD,
    EDIT_AI_URL,
    EDIT_HA_URL,
    EDIT_HA_TOKEN,
} edit_field_t;

static edit_field_t s_edit_field = EDIT_NONE;
static lv_obj_t *s_editor_overlay = NULL;
static char s_wifi_ssid[33] = "";
static char s_wifi_password[65] = "";
static char s_ai_url[128] = "";
static char s_ha_url[128] = "";
static char s_ha_token[256] = "";

/* UI-only state for now. Networking/AI/BLE modules inject real state later. */
static bool s_wifi_enabled = true;
static bool s_wifi_configured = false;
static bool s_wifi_connected = false;
static bool s_ai_configured = false;
static bool s_ai_online = false;

static bool s_bt_enabled = true;
static bool s_bt_scan_view = false;
static bool s_bt_scanning = false;
static bool s_bt_connected_visible = true;
static bool s_bt_connected_selected = false;
static int32_t s_bt_pairing_available = -1;

static const char *s_bt_available_names[] = {
    "BLE Remote",
    "Room Sensor",
    "Desk Light",
    "Smart Button",
    "Thermo Sensor",
    "BLE Controller",
};

static const char *s_bt_available_rssi[] = {
    "-58 dBm",
    "-72 dBm",
    "-81 dBm",
    "-63 dBm",
    "-76 dBm",
    "-69 dBm",
};

#define UI_BT_AVAILABLE_COUNT ((int)(sizeof(s_bt_available_names) / sizeof(s_bt_available_names[0])))

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
        if (s_slider_ctx.tab == UI_SETTINGS_SOUND) {
            s_sound_value = value;
        }
        else if (s_slider_ctx.tab == UI_SETTINGS_DISPLAY) {
            s_brightness_value = value;
            /* Display brightness is a real board service now, not a preview-only
             * UI value. Keep the UI responsive even if hardware reports an error. */
            (void)board_backlight_set_percent((uint8_t)value);
        }
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

/* Scannable Wi-Fi QR generated for:
 *     WIFI:T:nopass;S:AI-Robot-Setup;;
 * The setup AP is intentionally fixed for this single-device product so the
 * QR can be a tiny static bitmap instead of pulling a QR encoder into RAM. */
static void build_setup_qr(lv_obj_t *parent, int32_t x, int32_t y)
{
    static const uint32_t rows[29] = {
        0x1FC5EF7Fu, 0x1044F841u, 0x1755A25Du, 0x175C685Du, 0x1759335Du,
        0x105E0141u, 0x1FD5557Fu, 0x00155700u, 0x17C41E7Cu, 0x1B23EF76u,
        0x1A57F0A8u, 0x0118A253u, 0x164F61ACu, 0x01393176u, 0x0EDC0DF4u,
        0x140F5158u, 0x0ACC0A0Bu, 0x1784E63Au, 0x1064F190u, 0x17258051u,
        0x13735BFCu, 0x00140114u, 0x1FC73154u, 0x105F471Au, 0x175E19F6u,
        0x175042AFu, 0x175C6076u, 0x1047E0EAu, 0x1FD8FAE4u
    };
    const int32_t cell = 3;
    const int32_t quiet = 4;
    const int32_t modules = 29 + quiet * 2;
    const int32_t size = modules * cell;

    lv_obj_t *plate = plain_obj(parent);
    lv_obj_set_size(plate, size, size);
    lv_obj_set_pos(plate, x, y);
    lv_obj_set_style_bg_color(plate, UI_COLOR_FG, 0);
    lv_obj_set_style_bg_opa(plate, LV_OPA_COVER, 0);

    for(int32_t row = 0; row < 29; ++row) {
        for(int32_t col = 0; col < 29; ++col) {
            if(((rows[row] >> (28 - col)) & 1u) == 0) continue;
            lv_obj_t *dot = plain_obj(plate);
            lv_obj_set_size(dot, cell, cell);
            lv_obj_set_pos(dot, (quiet + col) * cell, (quiet + row) * cell);
            lv_obj_set_style_bg_color(dot, UI_COLOR_BG, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        }
    }
}

static void ensure_setup_portal(void)
{
#if defined(ESP_PLATFORM)
    (void)network_service_start_setup_portal();
#endif
}

static void wifi_toggle_event_cb(lv_event_t *e);
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
#if defined(ESP_PLATFORM)
    (void)network_service_set_wifi_enabled(s_wifi_enabled);
#endif
    if (!s_wifi_enabled) s_wifi_connected = false;
    note_activity();
    show_tab(UI_SETTINGS_WIFI);
}

static lv_obj_t *build_bt_row(lv_obj_t *parent, const char *name,
                              int32_t y, int32_t w, bool active)
{
    lv_obj_t *row = plain_obj(parent);
    lv_obj_set_size(row, w, 28);
    lv_obj_set_pos(row, 0, y);
    lv_obj_set_style_bg_color(row, active ? UI_COLOR_PANEL_2 : UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = make_label(row, name, active ? UI_COLOR_FG : UI_COLOR_MUTED);
    lv_obj_set_pos(label, 10, 5);
    return row;
}

static lv_obj_t *build_bt_scroll_list(lv_obj_t *parent, int32_t x, int32_t y,
                                      int32_t w, int32_t h)
{
    lv_obj_t *list = plain_obj(parent);
    lv_obj_set_size(list, w, h);
    lv_obj_set_pos(list, x, y);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    return list;
}

static void build_bluetooth_content(void)
{
    lv_obj_t *icon = ui_system_icon_bluetooth(
        s_content, s_bt_enabled ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(icon, 61, 22);

    lv_obj_t *state = make_label(s_content,
                                 s_bt_enabled ? "ON" : "OFF",
                                 s_bt_enabled ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(state, 67, 78);

    lv_obj_t *hit = plain_obj(s_content);
    lv_obj_set_size(hit, 118, 100);
    lv_obj_set_pos(hit, 18, 8);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, bt_icon_event_cb, LV_EVENT_CLICKED, NULL);

    add_status_divider(s_content);

    lv_obj_t *title = make_label(s_content,
                                 s_bt_enabled ? "BLUETOOTH READY" : "BLUETOOTH DISABLED",
                                 s_bt_enabled ? UI_COLOR_FG : UI_COLOR_MUTED);
    lv_obj_set_pos(title, UI_STATUS_RIGHT_X + 22, 24);

    lv_obj_t *detail = make_label(
        s_content,
        s_bt_enabled
            ? "Real device scanning/pairing will use the NimBLE service."
            : "Tap the Bluetooth icon to enable.",
        UI_COLOR_MUTED);
    lv_obj_set_pos(detail, UI_STATUS_RIGHT_X + 22, 54);
    lv_obj_set_width(detail, UI_STATUS_RIGHT_W - 44);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);

    if(s_bt_enabled) {
        lv_obj_t *scan = add_action_button(s_content, "SCAN",
                                           UI_STATUS_RIGHT_X + 22, 88, 92,
                                           UI_COLOR_PANEL_2, UI_COLOR_MUTED);
        lv_obj_add_state(scan, LV_STATE_DISABLED);
    }
}

static void bt_icon_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_bt_enabled = !s_bt_enabled;
    note_activity();
    show_tab(UI_SETTINGS_BLUETOOTH);
}

static void bt_scan_event_cb(lv_event_t *e) { (void)e; }
static void bt_connected_event_cb(lv_event_t *e) { (void)e; }
static void bt_forget_event_cb(lv_event_t *e) { (void)e; }
static void bt_available_event_cb(lv_event_t *e) { (void)e; }

static void build_ai_content(void)
{
    load_runtime_settings();
    ensure_setup_portal();

    lv_obj_t *icon = ui_system_icon_ai_robot2(
        s_content, s_ai_configured ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(icon, 61, 18);

    lv_obj_t *state = make_label(
        s_content,
        s_ai_configured ? "CONFIGURED" : "NOT SET",
        s_ai_configured ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(state, 43, 72);

    add_status_divider(s_content);
    build_setup_qr(s_content, UI_STATUS_RIGHT_X + 12, 6);

    lv_obj_t *title = make_label(s_content, "PHONE WEB SETUP", UI_COLOR_ACCENT);
    lv_obj_set_pos(title, UI_STATUS_RIGHT_X + 137, 13);

    lv_obj_t *line1 = make_label(s_content, "AI Server + Home Assistant", UI_COLOR_FG);
    lv_obj_set_pos(line1, UI_STATUS_RIGHT_X + 137, 38);

    lv_obj_t *line2 = make_label(s_content, "HA URL + Long-Lived Token", UI_COLOR_MUTED);
    lv_obj_set_pos(line2, UI_STATUS_RIGHT_X + 137, 61);

    lv_obj_t *line3 = make_label(s_content, "AP: AI-Robot-Setup", UI_COLOR_MUTED);
    lv_obj_set_pos(line3, UI_STATUS_RIGHT_X + 137, 84);
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
    load_runtime_settings();
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
    close_editor();
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
