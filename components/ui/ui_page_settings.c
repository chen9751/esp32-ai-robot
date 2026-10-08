#include "ui_page_settings.h"
#include "ui_system_icons.h"
#include <stdbool.h>
#if defined(ESP_PLATFORM)
#include "board.h"
#include "network_service.h"
#include "bluetooth_service.h"
#include "audio_service.h"
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

#define BLUETOOTH_MAX_SCAN_RESULTS 8
#define BLUETOOTH_DEVICE_NAME_MAX 32
#define BLUETOOTH_ADDRESS_STR_MAX 18
typedef enum {
    BLUETOOTH_LINK_IDLE = 0,
    BLUETOOTH_LINK_SCANNING,
    BLUETOOTH_LINK_CONNECTING,
    BLUETOOTH_LINK_PAIRING,
    BLUETOOTH_LINK_CONNECTED,
} bluetooth_link_state_t;
typedef struct {
    char name[BLUETOOTH_DEVICE_NAME_MAX];
    char address[BLUETOOTH_ADDRESS_STR_MAX];
    int8_t rssi;
    uint8_t addr_type;
    uint8_t addr[6];
} bluetooth_scan_result_t;
typedef struct {
    bool initialized;
    bool ready;
    bool enabled;
    bool scanning;
    bool connected;
    bool bonded;
    bluetooth_link_state_t state;
    char peer_name[BLUETOOTH_DEVICE_NAME_MAX];
    char peer_address[BLUETOOTH_ADDRESS_STR_MAX];
    int last_error;
    uint32_t generation;
} bluetooth_status_t;

static int board_backlight_set_percent(unsigned char percent)
{
    (void)percent;
    return 0;
}
static int audio_service_set_volume(unsigned char percent)
{
    (void)percent;
    return 0;
}
#endif

#include <stdint.h>
#include <string.h>

#define UI_SCREEN_W               640
#define UI_SCREEN_H               172
#define UI_CONTENT_X               28  /* Center the 584px content, no back rail. */
#define UI_CONTENT_W              (UI_SCREEN_W - 2 * UI_CONTENT_X)
#define UI_CONTENT_H              124
#define UI_NAV_Y                  124
#define UI_TAB_COUNT                6
#define UI_TAB_W                  106
#define UI_TAB_LAST_W             110
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
    UI_SETTINGS_BLUETOOTH,
    UI_SETTINGS_WIFI,
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
static int32_t s_sound_value = 30;
static int32_t s_brightness_value = 20;

/* Runtime state mirrored from the network service for rendering. */
static bool s_wifi_enabled = true;
static bool s_wifi_configured = false;
static bool s_wifi_connected = false;
static bool s_ai_configured = false;
static bool s_bt_enabled = false;
static lv_timer_t *s_bt_refresh_timer = NULL;
static uint32_t s_bt_last_generation = UINT32_MAX;
static lv_timer_t *s_system_refresh_timer = NULL;
static lv_obj_t *s_system_battery_value = NULL;

static void show_tab(ui_settings_tab_t tab);
static void refresh_tab_styles(void);
static void build_bluetooth_content(void);
static void bt_icon_event_cb(lv_event_t *e);
static void bt_scan_event_cb(lv_event_t *e);
static void bt_device_event_cb(lv_event_t *e);
static void bt_disconnect_event_cb(lv_event_t *e);
static void bt_forget_event_cb(lv_event_t *e);
static void build_system_content(void);
static void stop_system_refresh_timer(void);

static void load_runtime_settings(void)
{
#if defined(ESP_PLATFORM)
    network_wifi_status_t status = {0};
    network_service_get_wifi_status(&status);
    s_wifi_enabled = status.enabled;
    s_wifi_configured = status.configured;
    s_wifi_connected = status.connected;

    network_backend_config_t backend = {0};
    if (network_service_get_backend_config(&backend) == ESP_OK) {
        s_ai_configured = backend.ai_url[0] != '\0' ||
                          backend.ha_url[0] != '\0' ||
                          backend.ha_token[0] != '\0';
    }
#else
    /* Web preview has no device network service. Keep its local defaults. */
    s_wifi_configured = false;
    s_wifi_connected = false;
    s_ai_configured = false;
#endif
}

static void note_activity(void)
{
    if (s_activity_cb != NULL) s_activity_cb(s_activity_user_data);
}

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
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
            (void)audio_service_set_volume((uint8_t)value);
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
    /* Slider drags belong exclusively to the control: never bubble their
     * vertical finger movement into the settings swipe-to-dismiss handler. */
    lv_obj_remove_flag(slider, LV_OBJ_FLAG_EVENT_BUBBLE);
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

static void build_wifi_content(void)
{
    load_runtime_settings();
    network_wifi_status_t status = {0};
#if defined(ESP_PLATFORM)
    network_service_get_wifi_status(&status);
#else
    status.enabled = s_wifi_enabled;
    status.configured = s_wifi_configured;
    status.connected = s_wifi_connected;
#endif
    lv_obj_t *icon = ui_system_icon_wifi(s_content,
        status.connected ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(icon, 61, 18);
    lv_obj_t *state = make_label(s_content,
        status.connected ? "ONLINE" : (status.configured ? "OFFLINE" : "NOT SET"),
        status.connected ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(state, 43, 72);
    add_status_divider(s_content);
    lv_obj_t *ssid_label = make_label(s_content, "SSID", UI_COLOR_MUTED);
    lv_obj_set_pos(ssid_label, UI_STATUS_RIGHT_X + 18, 12);
    lv_obj_t *ssid = make_label(s_content,
        status.configured ? status.ssid : "NOT SET", UI_COLOR_FG);
    lv_obj_set_pos(ssid, UI_STATUS_RIGHT_X + 100, 12);
    lv_obj_set_width(ssid, 270);
    lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
    lv_obj_t *ip_label = make_label(s_content, "IP", UI_COLOR_MUTED);
    lv_obj_set_pos(ip_label, UI_STATUS_RIGHT_X + 18, 44);
    lv_obj_t *ip = make_label(s_content,
        status.connected ? status.ip : "--", UI_COLOR_FG);
    lv_obj_set_pos(ip, UI_STATUS_RIGHT_X + 100, 44);
    const char *diagnostic = status.time_synced ? "TIME SYNCED" : "TIME NOT SYNCED";
#if defined(ESP_PLATFORM)
    if (!status.configured) diagnostic = network_service_config_status();
#endif
    lv_obj_t *time = make_label(s_content, diagnostic,
        status.time_synced && status.configured ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(time, UI_STATUS_RIGHT_X + 18, 82);
}

static void get_bluetooth_status(bluetooth_status_t *status)
{
    memset(status, 0, sizeof(*status));
#if defined(ESP_PLATFORM)
    bluetooth_service_get_status(status);
#else
    status->initialized = true;
    status->ready = true;
    status->enabled = s_bt_enabled;
    status->generation = 1;
#endif
}

static size_t get_bluetooth_results(bluetooth_scan_result_t *results, size_t capacity)
{
#if defined(ESP_PLATFORM)
    return bluetooth_service_get_scan_results(results, capacity);
#else
    (void)results;
    (void)capacity;
    return 0;
#endif
}

static const char *bluetooth_state_text(const bluetooth_status_t *status)
{
    if(!status->enabled) return "OFF";
    if(!status->ready) return "STARTING";
    switch(status->state) {
        case BLUETOOTH_LINK_SCANNING:   return "SCANNING";
        case BLUETOOTH_LINK_CONNECTING: return "CONNECTING";
        case BLUETOOTH_LINK_PAIRING:    return "PAIRING";
        case BLUETOOTH_LINK_CONNECTED:  return status->bonded ? "PAIRED" : "CONNECTED";
        case BLUETOOTH_LINK_IDLE:
        default:                        return "READY";
    }
}

static void rebuild_bluetooth_content(void)
{
    if(s_content == NULL || s_selected != UI_SETTINGS_BLUETOOTH) return;
    lv_obj_clean(s_content);
    build_bluetooth_content();
    refresh_tab_styles();
}

static void bt_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if(s_selected != UI_SETTINGS_BLUETOOTH) return;

    bluetooth_status_t status = {0};
    get_bluetooth_status(&status);
    if(status.generation == s_bt_last_generation) return;

    s_bt_last_generation = status.generation;
    rebuild_bluetooth_content();
}

static void start_bt_refresh_timer(void)
{
    if(s_bt_refresh_timer == NULL) {
        s_bt_refresh_timer = lv_timer_create(bt_refresh_timer_cb, 400, NULL);
    }
}

static void stop_bt_refresh_timer(void)
{
    if(s_bt_refresh_timer != NULL) {
        lv_timer_delete(s_bt_refresh_timer);
        s_bt_refresh_timer = NULL;
    }
}

static void build_bluetooth_content(void)
{
    bluetooth_status_t status = {0};
    get_bluetooth_status(&status);
    s_bt_enabled = status.enabled;
    s_bt_last_generation = status.generation;

    lv_obj_t *icon = ui_system_icon_bluetooth(
        s_content, status.enabled ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(icon, 61, 18);

    lv_obj_t *state = make_label(s_content, bluetooth_state_text(&status),
                                 (status.enabled && status.ready)
                                     ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(state, 48, 72);

    lv_obj_t *hit = plain_obj(s_content);
    lv_obj_set_size(hit, 118, 104);
    lv_obj_set_pos(hit, 18, 6);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, bt_icon_event_cb, LV_EVENT_CLICKED, NULL);

    add_status_divider(s_content);

    if(!status.enabled) {
        lv_obj_t *title = make_label(s_content, "BLUETOOTH OFF", UI_COLOR_MUTED);
        lv_obj_set_pos(title, UI_STATUS_RIGHT_X + 22, 28);
        lv_obj_t *hint = make_label(s_content,
                                    "Tap the Bluetooth icon to enable.",
                                    UI_COLOR_MUTED);
        lv_obj_set_pos(hint, UI_STATUS_RIGHT_X + 22, 56);
        start_bt_refresh_timer();
        return;
    }

    if(!status.ready) {
        lv_obj_t *title = make_label(s_content, "STARTING BLUETOOTH", UI_COLOR_FG);
        lv_obj_set_pos(title, UI_STATUS_RIGHT_X + 22, 28);
        lv_obj_t *hint = make_label(s_content, "NimBLE host is starting...", UI_COLOR_MUTED);
        lv_obj_set_pos(hint, UI_STATUS_RIGHT_X + 22, 56);
        start_bt_refresh_timer();
        return;
    }

    if(status.connected) {
        lv_obj_t *name = make_label(
            s_content,
            status.peer_name[0] ? status.peer_name : "BLE device",
            UI_COLOR_FG);
        lv_obj_set_pos(name, UI_STATUS_RIGHT_X + 16, 14);

        lv_obj_t *addr = make_label(s_content, status.peer_address, UI_COLOR_MUTED);
        lv_obj_set_pos(addr, UI_STATUS_RIGHT_X + 16, 38);

        lv_obj_t *security = make_label(
            s_content,
            status.state == BLUETOOTH_LINK_PAIRING
                ? "PAIRING..."
                : (status.bonded ? "BONDED" : "LINK CONNECTED"),
            status.bonded ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
        lv_obj_set_pos(security, UI_STATUS_RIGHT_X + 16, 62);

        lv_obj_t *disconnect = add_action_button(
            s_content, "DISCONNECT",
            UI_STATUS_RIGHT_X + 16, 88, 105,
            UI_COLOR_PANEL_2, UI_COLOR_FG);
        lv_obj_add_event_cb(disconnect, bt_disconnect_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *forget = add_action_button(
            s_content, "FORGET",
            UI_STATUS_RIGHT_X + 132, 88, 80,
            UI_COLOR_PANEL_2, UI_COLOR_DANGER);
        lv_obj_add_event_cb(forget, bt_forget_event_cb, LV_EVENT_CLICKED, NULL);

        start_bt_refresh_timer();
        return;
    }

    lv_obj_t *scan = add_action_button(
        s_content,
        status.scanning ? "SCANNING..." : "SCAN",
        UI_STATUS_RIGHT_X + 14, 8, 96,
        status.scanning ? UI_COLOR_TAB_ACTIVE : UI_COLOR_PANEL_2,
        status.scanning ? UI_COLOR_MUTED : UI_COLOR_ACCENT);
    if(!status.scanning) {
        lv_obj_add_event_cb(scan, bt_scan_event_cb, LV_EVENT_CLICKED, NULL);
    }

    if(status.last_error != 0 && !status.scanning) {
        lv_obj_t *err = make_label(s_content, "LAST CONNECT/SCAN FAILED", UI_COLOR_DANGER);
        lv_obj_set_pos(err, UI_STATUS_RIGHT_X + 126, 13);
    }
    else {
        lv_obj_t *hint = make_label(
            s_content,
            status.scanning ? "Searching nearby BLE devices" : "Tap a device to connect",
            UI_COLOR_MUTED);
        lv_obj_set_pos(hint, UI_STATUS_RIGHT_X + 126, 13);
    }

    bluetooth_scan_result_t results[BLUETOOTH_MAX_SCAN_RESULTS] = {0};
    size_t count = get_bluetooth_results(results, BLUETOOTH_MAX_SCAN_RESULTS);

    lv_obj_t *list = plain_obj(s_content);
    lv_obj_set_pos(list, UI_STATUS_RIGHT_X + 14, 40);
    lv_obj_set_size(list, UI_STATUS_RIGHT_W - 28, 76);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    if(count == 0) {
        lv_obj_t *empty = make_label(
            list,
            status.scanning ? "Scanning..." : "No scan results yet",
            UI_COLOR_MUTED);
        lv_obj_set_pos(empty, 8, 14);
    }
    else {
        for(size_t i = 0; i < count; ++i) {
            lv_obj_t *row = plain_obj(list);
            lv_obj_set_pos(row, 0, (int32_t)i * 32);
            lv_obj_set_size(row, UI_STATUS_RIGHT_W - 42, 29);
            lv_obj_set_style_bg_color(row, UI_COLOR_PANEL, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(row, 7, 0);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row, bt_device_event_cb, LV_EVENT_CLICKED,
                                (void *)(uintptr_t)i);

            lv_obj_t *name = make_label(row, results[i].name, UI_COLOR_FG);
            lv_obj_set_pos(name, 8, 5);
            lv_obj_set_width(name, 190);
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

            lv_obj_t *addr = make_label(row, results[i].address, UI_COLOR_MUTED);
            lv_obj_set_pos(addr, 205, 5);

            lv_obj_t *rssi = lv_label_create(row);
            lv_label_set_text_fmt(rssi, "%d", (int)results[i].rssi);
            lv_obj_set_style_text_color(rssi, UI_COLOR_MUTED, 0);
            lv_obj_set_pos(rssi, 340, 5);
        }
    }

    start_bt_refresh_timer();
}

static void bt_icon_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    bluetooth_status_t status = {0};
    get_bluetooth_status(&status);
#if defined(ESP_PLATFORM)
    esp_err_t bt_err = bluetooth_service_set_enabled(!status.enabled);
    if (bt_err != ESP_OK) {
        /* Do not fake an ON state if lazy controller allocation fails. */
        ESP_LOGW("settings", "Bluetooth enable failed: %s", esp_err_to_name(bt_err));
    }
#else
    s_bt_enabled = !status.enabled;
#endif
    note_activity();
    s_bt_last_generation = UINT32_MAX;
    rebuild_bluetooth_content();
}

static void bt_scan_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
#if defined(ESP_PLATFORM)
    (void)bluetooth_service_start_scan();
#endif
    note_activity();
    s_bt_last_generation = UINT32_MAX;
    rebuild_bluetooth_content();
}

static void bt_device_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(e);
#if defined(ESP_PLATFORM)
    (void)bluetooth_service_connect(index);
#else
    (void)index;
#endif
    note_activity();
    s_bt_last_generation = UINT32_MAX;
    rebuild_bluetooth_content();
}

static void bt_disconnect_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
#if defined(ESP_PLATFORM)
    (void)bluetooth_service_disconnect();
#endif
    note_activity();
}

static void bt_forget_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
#if defined(ESP_PLATFORM)
    (void)bluetooth_service_forget_peer();
    (void)bluetooth_service_disconnect();
#endif
    note_activity();
}

static void build_ai_content(void)
{
    load_runtime_settings();
    network_backend_config_t backend = {0};
#if defined(ESP_PLATFORM)
    (void)network_service_get_backend_config(&backend);
#endif
    lv_obj_t *icon = ui_system_icon_ai_robot2(s_content,
        s_ai_configured ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(icon, 61, 18);
    lv_obj_t *state = make_label(s_content,
        s_ai_configured ? "CONFIGURED" : "NOT SET",
        s_ai_configured ? UI_COLOR_ACCENT : UI_COLOR_MUTED);
    lv_obj_set_pos(state, 43, 72);
    add_status_divider(s_content);
    lv_obj_t *ai_label = make_label(s_content, "AI SERVER", UI_COLOR_MUTED);
    lv_obj_set_pos(ai_label, UI_STATUS_RIGHT_X + 18, 12);
    lv_obj_t *ai_value = make_label(s_content,
        backend.ai_url[0] ? backend.ai_url : "NOT SET", UI_COLOR_FG);
    lv_obj_set_pos(ai_value, UI_STATUS_RIGHT_X + 18, 40);
    lv_obj_set_width(ai_value, UI_STATUS_RIGHT_W - 30);
    lv_label_set_long_mode(ai_value, LV_LABEL_LONG_DOT);
    lv_obj_t *ha_label = make_label(s_content, "HA", UI_COLOR_MUTED);
    lv_obj_set_pos(ha_label, UI_STATUS_RIGHT_X + 18, 80);
    lv_obj_t *ha_value = make_label(s_content,
        backend.ha_url[0] ? backend.ha_url : "NOT SET", UI_COLOR_FG);
    lv_obj_set_pos(ha_value, UI_STATUS_RIGHT_X + 65, 80);
    lv_obj_set_width(ha_value, UI_STATUS_RIGHT_W - 80);
    lv_label_set_long_mode(ha_value, LV_LABEL_LONG_DOT);
}

static void refresh_system_battery_value(void)
{
    if (s_system_battery_value == NULL) return;

#if defined(ESP_PLATFORM)
    board_power_status_t power = {0};
    if (board_power_get_status(&power) == ESP_OK && power.available) {
        const unsigned volts = power.battery_mv / 1000U;
        const unsigned centivolts = (power.battery_mv % 1000U) / 10U;
        lv_label_set_text_fmt(s_system_battery_value, "%u.%02u V",
                              volts, centivolts);
        lv_obj_set_style_text_color(
            s_system_battery_value,
            power.low_battery ? UI_COLOR_DANGER : UI_COLOR_FG,
            0);
        return;
    }
#endif

    lv_label_set_text(s_system_battery_value, "--.-- V");
    lv_obj_set_style_text_color(s_system_battery_value, UI_COLOR_MUTED, 0);
}

static void system_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_selected != UI_SETTINGS_SYSTEM) return;
    refresh_system_battery_value();
}

static void start_system_refresh_timer(void)
{
    if (s_system_refresh_timer == NULL) {
        s_system_refresh_timer = lv_timer_create(system_refresh_timer_cb, 1000, NULL);
    }
}

static void stop_system_refresh_timer(void)
{
    if (s_system_refresh_timer != NULL) {
        lv_timer_delete(s_system_refresh_timer);
        s_system_refresh_timer = NULL;
    }
    s_system_battery_value = NULL;
}

static void build_system_content(void)
{
    lv_obj_t *title = make_label(s_content, "SYSTEM", UI_COLOR_ACCENT);
    lv_obj_set_pos(title, 22, 12);

    lv_obj_t *row = plain_obj(s_content);
    lv_obj_set_pos(row, 18, 42);
    lv_obj_set_size(row, UI_CONTENT_W - 36, 48);
    lv_obj_set_style_bg_color(row, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 10, 0);

    lv_obj_t *label = make_label(row, "BATTERY", UI_COLOR_FG);
    lv_obj_set_pos(label, 16, 14);

    s_system_battery_value = make_label(row, "--.-- V", UI_COLOR_FG);
    lv_obj_align(s_system_battery_value, LV_ALIGN_RIGHT_MID, -16, 0);

    refresh_system_battery_value();
    start_system_refresh_timer();
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
    if (tab != UI_SETTINGS_BLUETOOTH) stop_bt_refresh_timer();
    if (tab != UI_SETTINGS_SYSTEM) stop_system_refresh_timer();
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
    else if (tab == UI_SETTINGS_SYSTEM) {
        build_system_content();
    }

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
    int32_t x = 0; /* Six tabs span all 640 pixels. */
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
    stop_bt_refresh_timer();
    stop_system_refresh_timer();
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
