#include "ui_page_alarm.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define UI_SCREEN_W              640
#define UI_SCREEN_H              172
#define UI_BACK_RAIL_W            56
#define UI_CONTENT_W             (UI_SCREEN_W - UI_BACK_RAIL_W)
#define UI_LIST_H                134
#define UI_ADD_BAR_H              38
#define UI_ALARM_MAX              16
#define UI_ALARM_ROW_H            48

#define UI_COLOR_BG          lv_color_hex(0x000000)
#define UI_COLOR_FG          lv_color_hex(0xF4F7F8)
#define UI_COLOR_MUTED       lv_color_hex(0x777D84)
#define UI_COLOR_PANEL       lv_color_hex(0x111419)
#define UI_COLOR_PANEL_2     lv_color_hex(0x1A1F25)
#define UI_COLOR_ACCENT      lv_color_hex(0x45D7F0)
#define UI_COLOR_LINE        lv_color_hex(0x262B31)

typedef enum {
    UI_ALARM_REPEAT_NEVER = 0,
    UI_ALARM_REPEAT_DAILY,
    UI_ALARM_REPEAT_CUSTOM,
} ui_alarm_repeat_t;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    ui_alarm_repeat_t repeat;
    uint8_t custom_days; /* bit 0..6 = Mon..Sun */
    bool enabled;
} ui_alarm_item_t;

static ui_alarm_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static lv_obj_t *s_root = NULL;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_modal = NULL;
static lv_obj_t *s_hour_roller = NULL;
static lv_obj_t *s_minute_roller = NULL;
static lv_obj_t *s_repeat_buttons[3] = {0};
static lv_obj_t *s_day_buttons[7] = {0};
static lv_obj_t *s_days_row = NULL;
static ui_alarm_repeat_t s_repeat = UI_ALARM_REPEAT_NEVER;
static uint8_t s_custom_days = 0;
static ui_alarm_item_t s_alarms[UI_ALARM_MAX];
static uint8_t s_alarm_count = 0;

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

static const char *repeat_text(const ui_alarm_item_t *alarm)
{
    static char custom_buf[24];
    if (alarm->repeat == UI_ALARM_REPEAT_DAILY) return "每天";
    if (alarm->repeat == UI_ALARM_REPEAT_NEVER) return "永不";

    const char *days[7] = {"一", "二", "三", "四", "五", "六", "日"};
    custom_buf[0] = '\0';
    for (int i = 0; i < 7; ++i) {
        if ((alarm->custom_days & (1u << i)) == 0) continue;
        if (custom_buf[0] != '\0') strncat(custom_buf, " ", sizeof(custom_buf) - strlen(custom_buf) - 1);
        strncat(custom_buf, days[i], sizeof(custom_buf) - strlen(custom_buf) - 1);
    }
    if (custom_buf[0] == '\0') return "自定义";
    return custom_buf;
}

static void alarm_switch_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    intptr_t index = (intptr_t)lv_event_get_user_data(e);
    if (index < 0 || index >= s_alarm_count) return;

    lv_obj_t *sw = lv_event_get_target_obj(e);
    s_alarms[index].enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    note_activity();
}

static void rebuild_list(void)
{
    if (s_list == NULL) return;
    lv_obj_clean(s_list);

    for (uint8_t i = 0; i < s_alarm_count; ++i) {
        lv_obj_t *row = plain_obj(s_list);
        lv_obj_set_size(row, UI_CONTENT_W - 20, UI_ALARM_ROW_H);
        lv_obj_set_style_bg_color(row, UI_COLOR_PANEL, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 10, 0);

        char time_buf[8];
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u", s_alarms[i].hour, s_alarms[i].minute);
        lv_obj_t *time = make_label(row, time_buf, UI_COLOR_FG);
        lv_obj_set_pos(time, 14, 7);

        lv_obj_t *repeat = make_label(row, repeat_text(&s_alarms[i]), UI_COLOR_MUTED);
        lv_obj_set_pos(repeat, 14, 26);

        lv_obj_t *sw = lv_switch_create(row);
        lv_obj_set_size(sw, 46, 24);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -12, 0);
        lv_obj_set_style_bg_color(sw, UI_COLOR_PANEL_2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, UI_COLOR_ACCENT, LV_PART_INDICATOR);
        if (s_alarms[i].enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_add_event_cb(sw, alarm_switch_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
    }
}

static void refresh_repeat_buttons(void)
{
    for (int i = 0; i < 3; ++i) {
        bool selected = (i == (int)s_repeat);
        lv_obj_set_style_bg_color(s_repeat_buttons[i], selected ? UI_COLOR_ACCENT : UI_COLOR_PANEL_2, 0);
        lv_obj_set_style_text_color(s_repeat_buttons[i], selected ? UI_COLOR_BG : UI_COLOR_FG, 0);
    }

    if (s_days_row != NULL) {
        if (s_repeat == UI_ALARM_REPEAT_CUSTOM) lv_obj_clear_flag(s_days_row, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_days_row, LV_OBJ_FLAG_HIDDEN);
    }
}

static void repeat_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    intptr_t repeat = (intptr_t)lv_event_get_user_data(e);
    if (repeat < UI_ALARM_REPEAT_NEVER || repeat > UI_ALARM_REPEAT_CUSTOM) return;
    s_repeat = (ui_alarm_repeat_t)repeat;
    refresh_repeat_buttons();
    note_activity();
}

static void day_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    intptr_t day = (intptr_t)lv_event_get_user_data(e);
    if (day < 0 || day > 6) return;

    s_custom_days ^= (uint8_t)(1u << day);
    bool selected = (s_custom_days & (1u << day)) != 0;
    lv_obj_t *btn = lv_event_get_target_obj(e);
    lv_obj_set_style_bg_color(btn, selected ? UI_COLOR_ACCENT : UI_COLOR_PANEL_2, 0);
    lv_obj_set_style_text_color(btn, selected ? UI_COLOR_BG : UI_COLOR_FG, 0);
    note_activity();
}

static void close_modal(void)
{
    if (s_modal != NULL) lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

static void add_alarm_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (s_alarm_count >= UI_ALARM_MAX) {
        close_modal();
        return;
    }

    ui_alarm_item_t *alarm = &s_alarms[s_alarm_count++];
    alarm->hour = (uint8_t)lv_roller_get_selected(s_hour_roller);
    alarm->minute = (uint8_t)lv_roller_get_selected(s_minute_roller);
    alarm->repeat = s_repeat;
    alarm->custom_days = s_custom_days;
    alarm->enabled = true;

    rebuild_list();
    close_modal();
    note_activity();
}

static void show_add_modal_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_repeat = UI_ALARM_REPEAT_NEVER;
    s_custom_days = 0;
    lv_roller_set_selected(s_hour_roller, 7, LV_ANIM_OFF);
    lv_roller_set_selected(s_minute_roller, 0, LV_ANIM_OFF);

    for (int i = 0; i < 7; ++i) {
        lv_obj_set_style_bg_color(s_day_buttons[i], UI_COLOR_PANEL_2, 0);
        lv_obj_set_style_text_color(s_day_buttons[i], UI_COLOR_FG, 0);
    }
    refresh_repeat_buttons();
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_modal);
    note_activity();
}

static lv_obj_t *create_small_button(lv_obj_t *parent, const char *text, int32_t w, int32_t h)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PANEL_2, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);

    lv_obj_t *label = make_label(btn, text, UI_COLOR_FG);
    lv_obj_center(label);
    return btn;
}

static void build_modal(lv_obj_t *parent)
{
    s_modal = plain_obj(parent);
    lv_obj_set_size(s_modal, UI_CONTENT_W, UI_SCREEN_H);
    lv_obj_set_pos(s_modal, UI_BACK_RAIL_W, 0);
    lv_obj_set_style_bg_color(s_modal, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *panel = plain_obj(s_modal);
    lv_obj_set_size(panel, UI_CONTENT_W - 14, UI_SCREEN_H - 8);
    lv_obj_set_pos(panel, 7, 4);
    lv_obj_set_style_bg_color(panel, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 14, 0);

    static const char *hours =
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";
    static const char *minutes =
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";

    s_hour_roller = lv_roller_create(panel);
    lv_roller_set_options(s_hour_roller, hours, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(s_hour_roller, 3);
    lv_obj_set_size(s_hour_roller, 84, 76);
    lv_obj_set_pos(s_hour_roller, 18, 7);
    lv_obj_set_style_bg_color(s_hour_roller, UI_COLOR_PANEL_2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_hour_roller, UI_COLOR_ACCENT, LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_hour_roller, UI_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hour_roller, UI_COLOR_BG, LV_PART_SELECTED);

    s_minute_roller = lv_roller_create(panel);
    lv_roller_set_options(s_minute_roller, minutes, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(s_minute_roller, 3);
    lv_obj_set_size(s_minute_roller, 84, 76);
    lv_obj_set_pos(s_minute_roller, 112, 7);
    lv_obj_set_style_bg_color(s_minute_roller, UI_COLOR_PANEL_2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_minute_roller, UI_COLOR_ACCENT, LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_minute_roller, UI_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_minute_roller, UI_COLOR_BG, LV_PART_SELECTED);

    lv_obj_t *repeat_label = make_label(panel, "重复", UI_COLOR_MUTED);
    lv_obj_set_pos(repeat_label, 218, 7);

    const char *repeat_names[3] = {"永不", "每天", "自定义"};
    for (int i = 0; i < 3; ++i) {
        s_repeat_buttons[i] = create_small_button(panel, repeat_names[i], 82, 28);
        lv_obj_set_pos(s_repeat_buttons[i], 218 + i * 88, 28);
        lv_obj_add_event_cb(s_repeat_buttons[i], repeat_button_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    s_days_row = plain_obj(panel);
    lv_obj_set_size(s_days_row, 348, 30);
    lv_obj_set_pos(s_days_row, 218, 61);
    const char *day_names[7] = {"一", "二", "三", "四", "五", "六", "日"};
    for (int i = 0; i < 7; ++i) {
        s_day_buttons[i] = create_small_button(s_days_row, day_names[i], 42, 28);
        lv_obj_set_pos(s_day_buttons[i], i * 49, 0);
        lv_obj_add_event_cb(s_day_buttons[i], day_button_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *divider = plain_obj(panel);
    lv_obj_set_size(divider, 348, 1);
    lv_obj_set_pos(divider, 218, 98);
    lv_obj_set_style_bg_color(divider, UI_COLOR_LINE, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

    lv_obj_t *add = create_small_button(panel, "Add", 348, 42);
    lv_obj_set_pos(add, 218, 108);
    lv_obj_set_style_bg_color(add, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_color(add, UI_COLOR_BG, 0);
    lv_obj_add_event_cb(add, add_alarm_event_cb, LV_EVENT_CLICKED, NULL);

    refresh_repeat_buttons();
}

lv_obj_t *ui_page_alarm_build(lv_obj_t *parent,
                              ui_alarm_activity_cb_t activity_cb,
                              void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;

    s_root = plain_obj(parent);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    s_list = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, UI_CONTENT_W, UI_LIST_H);
    lv_obj_set_pos(s_list, UI_BACK_RAIL_W, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_left(s_list, 10, 0);
    lv_obj_set_style_pad_right(s_list, 10, 0);
    lv_obj_set_style_pad_top(s_list, 7, 0);
    lv_obj_set_style_pad_bottom(s_list, 7, 0);
    lv_obj_set_style_pad_row(s_list, 7, 0);

    lv_obj_t *add_bar = plain_obj(s_root);
    lv_obj_set_size(add_bar, UI_CONTENT_W, UI_ADD_BAR_H);
    lv_obj_set_pos(add_bar, UI_BACK_RAIL_W, UI_LIST_H);
    lv_obj_set_style_bg_color(add_bar, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(add_bar, LV_OPA_COVER, 0);

    lv_obj_t *plus = lv_button_create(add_bar);
    lv_obj_set_size(plus, 52, 30);
    lv_obj_align(plus, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(plus, UI_COLOR_PANEL_2, 0);
    lv_obj_set_style_bg_opa(plus, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(plus, 15, 0);
    lv_obj_set_style_border_width(plus, 0, 0);
    lv_obj_t *plus_label = make_label(plus, "+", UI_COLOR_FG);
    lv_obj_center(plus_label);
    lv_obj_add_event_cb(plus, show_add_modal_event_cb, LV_EVENT_CLICKED, NULL);

    build_modal(s_root);
    rebuild_list();
    return s_root;
}

void ui_page_alarm_stop(void)
{
    if (s_root != NULL) lv_obj_delete(s_root);

    s_root = NULL;
    s_list = NULL;
    s_modal = NULL;
    s_hour_roller = NULL;
    s_minute_roller = NULL;
    s_days_row = NULL;
    memset(s_repeat_buttons, 0, sizeof(s_repeat_buttons));
    memset(s_day_buttons, 0, sizeof(s_day_buttons));
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}
