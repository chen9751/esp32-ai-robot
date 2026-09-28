#include "ui_page_alarm.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define UI_SCREEN_W              640
#define UI_SCREEN_H              172
#define UI_BACK_RAIL_W            56
#define UI_CONTENT_W             (UI_SCREEN_W - UI_BACK_RAIL_W)
#define UI_LIST_H                124
#define UI_ADD_CARD_W             97
#define UI_ADD_CARD_H             48
#define UI_ALARM_MAX              16
#define UI_ALARM_ROW_H            52

#define UI_EDITOR_PANEL_W        576
#define UI_EDITOR_PANEL_H        164
#define UI_EDITOR_PANEL_X          4
#define UI_EDITOR_PANEL_Y          4
#define UI_ROLLER_W              164
#define UI_ROLLER_H              152
#define UI_ROLLER_1_X              8
#define UI_ROLLER_2_X            180
#define UI_RIGHT_X               354
#define UI_RIGHT_W               214
#define UI_EDIT_BUTTON_W         104
#define UI_EDIT_BUTTON_GAP         6

#define UI_COLOR_BG          lv_color_hex(0x000000)
#define UI_COLOR_FG          lv_color_hex(0xF4F7F8)
#define UI_COLOR_MUTED       lv_color_hex(0x89919A)
#define UI_COLOR_PANEL       lv_color_hex(0x0B0F13)
#define UI_COLOR_PANEL_2     lv_color_hex(0x151A20)
#define UI_COLOR_ACCENT      lv_color_hex(0x45D7F0)
#define UI_COLOR_LINE        lv_color_hex(0x293038)
#define UI_COLOR_TAB         lv_color_hex(0x0C0E11)
#define UI_COLOR_DANGER      lv_color_hex(0xE05A63)

#ifdef UI_ALARM_HAS_SOURCE_HAN
LV_FONT_DECLARE(ui_font_source_han_alarm_16);
#define UI_FONT_CN (&ui_font_source_han_alarm_16)
#else
#define UI_FONT_CN (&lv_font_simsun_16_cjk)
#endif

typedef enum {
    UI_ALARM_REPEAT_NEVER = 0,
    UI_ALARM_REPEAT_DAILY,
    UI_ALARM_REPEAT_CUSTOM,
} ui_alarm_repeat_t;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    ui_alarm_repeat_t repeat;
    uint8_t custom_days;
    bool enabled;
} ui_alarm_item_t;

static ui_alarm_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static lv_obj_t *s_root = NULL;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_editor = NULL;
static lv_obj_t *s_hour_roller = NULL;
static lv_obj_t *s_minute_roller = NULL;
static lv_obj_t *s_repeat_buttons[3] = {0};
static lv_obj_t *s_day_buttons[7] = {0};
static lv_obj_t *s_days_row = NULL;
static lv_obj_t *s_primary_button = NULL;
static lv_obj_t *s_primary_label = NULL;
static lv_obj_t *s_delete_button = NULL;
static ui_alarm_repeat_t s_repeat = UI_ALARM_REPEAT_NEVER;
static uint8_t s_custom_days = 0;
static int32_t s_edit_index = -1;
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
    lv_obj_set_style_text_font(label, UI_FONT_CN, 0);
    return label;
}

static lv_obj_t *make_latin_label(lv_obj_t *parent, const char *text,
                                  lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(label, font, 0);
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
        strncat(custom_buf, days[i], sizeof(custom_buf) - strlen(custom_buf) - 1);
    }
    return custom_buf[0] == '\0' ? "自定义" : custom_buf;
}

static void set_day_error(bool error)
{
    for (int i = 0; i < 7; ++i) {
        if (s_day_buttons[i] == NULL) continue;
        lv_obj_set_style_border_width(s_day_buttons[i], error ? 1 : 0, 0);
        lv_obj_set_style_border_color(s_day_buttons[i], UI_COLOR_DANGER, 0);
        lv_obj_set_style_border_opa(s_day_buttons[i], LV_OPA_COVER, 0);
    }
}

static void set_time_error(bool error)
{
    lv_obj_t *rollers[2] = {s_hour_roller, s_minute_roller};
    for (int i = 0; i < 2; ++i) {
        if (rollers[i] == NULL) continue;
        lv_obj_set_style_border_width(rollers[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(rollers[i],
                                      error ? UI_COLOR_DANGER : UI_COLOR_LINE,
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(rollers[i], LV_OPA_COVER, LV_PART_MAIN);
    }
}

static void roller_value_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    set_time_error(false);
    note_activity();
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

static void open_editor_for_index(int32_t index);

static void alarm_row_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    intptr_t index = (intptr_t)lv_event_get_user_data(e);
    if (index < 0 || index >= s_alarm_count) return;
    open_editor_for_index((int32_t)index);
}

static void sort_alarms(void)
{
    for (uint8_t i = 1; i < s_alarm_count; ++i) {
        ui_alarm_item_t key = s_alarms[i];
        int32_t j = (int32_t)i - 1;
        int key_minutes = key.hour * 60 + key.minute;
        while (j >= 0) {
            int current_minutes = s_alarms[j].hour * 60 + s_alarms[j].minute;
            if (current_minutes <= key_minutes) break;
            s_alarms[j + 1] = s_alarms[j];
            --j;
        }
        s_alarms[j + 1] = key;
    }
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
        lv_obj_set_style_radius(row, 11, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, alarm_row_event_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        char time_buf[8];
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u",
                 s_alarms[i].hour, s_alarms[i].minute);
        lv_obj_t *time = make_latin_label(row, time_buf, UI_COLOR_FG,
                                          &lv_font_montserrat_28);
        lv_obj_align(time, LV_ALIGN_LEFT_MID, 15, 0);

        lv_obj_t *repeat = make_label(row, repeat_text(&s_alarms[i]), UI_COLOR_MUTED);
        lv_obj_align(repeat, LV_ALIGN_LEFT_MID, 110, 1);

        lv_obj_t *sw = lv_switch_create(row);
        lv_obj_set_size(sw, 48, 26);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -13, 0);
        lv_obj_set_style_bg_color(sw, UI_COLOR_PANEL_2, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, UI_COLOR_ACCENT, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_EVENT_BUBBLE);
        if (s_alarms[i].enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_add_event_cb(sw, alarm_switch_event_cb, LV_EVENT_VALUE_CHANGED,
                            (void *)(intptr_t)i);
    }

    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
}

static void refresh_repeat_buttons(void)
{
    for (int i = 0; i < 3; ++i) {
        if (s_repeat_buttons[i] == NULL) continue;
        bool selected = (i == (int)s_repeat);
        lv_obj_set_style_bg_color(s_repeat_buttons[i],
                                  selected ? UI_COLOR_ACCENT : UI_COLOR_PANEL_2, 0);
        lv_obj_set_style_text_color(s_repeat_buttons[i],
                                    selected ? UI_COLOR_BG : UI_COLOR_FG, 0);
    }

    if (s_days_row != NULL) {
        if (s_repeat == UI_ALARM_REPEAT_CUSTOM) {
            lv_obj_clear_flag(s_days_row, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_days_row, LV_OBJ_FLAG_HIDDEN);
            set_day_error(false);
        }
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
    lv_obj_set_style_bg_color(btn,
                              selected ? UI_COLOR_ACCENT : UI_COLOR_PANEL_2, 0);
    lv_obj_set_style_text_color(btn,
                                selected ? UI_COLOR_BG : UI_COLOR_FG, 0);
    if (s_custom_days != 0) set_day_error(false);
    note_activity();
}

static void close_editor(void)
{
    if (s_editor != NULL) lv_obj_add_flag(s_editor, LV_OBJ_FLAG_HIDDEN);
    s_edit_index = -1;
    set_day_error(false);
    set_time_error(false);
}

static bool time_already_exists(uint8_t hour, uint8_t minute)
{
    for (uint8_t i = 0; i < s_alarm_count; ++i) {
        if ((int32_t)i == s_edit_index) continue;
        if (s_alarms[i].hour == hour && s_alarms[i].minute == minute) return true;
    }
    return false;
}

static bool editor_values_valid(void)
{
    bool valid = true;
    uint8_t hour = (uint8_t)lv_roller_get_selected(s_hour_roller);
    uint8_t minute = (uint8_t)lv_roller_get_selected(s_minute_roller);

    if (time_already_exists(hour, minute)) {
        set_time_error(true);
        valid = false;
    } else {
        set_time_error(false);
    }

    if (s_repeat == UI_ALARM_REPEAT_CUSTOM && s_custom_days == 0) {
        set_day_error(true);
        valid = false;
    } else {
        set_day_error(false);
    }
    return valid;
}

static void primary_action_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!editor_values_valid()) return;

    uint8_t hour = (uint8_t)lv_roller_get_selected(s_hour_roller);
    uint8_t minute = (uint8_t)lv_roller_get_selected(s_minute_roller);

    if (s_edit_index >= 0 && s_edit_index < s_alarm_count) {
        ui_alarm_item_t *alarm = &s_alarms[s_edit_index];
        alarm->hour = hour;
        alarm->minute = minute;
        alarm->repeat = s_repeat;
        alarm->custom_days = s_custom_days;
    } else if (s_alarm_count < UI_ALARM_MAX) {
        ui_alarm_item_t *alarm = &s_alarms[s_alarm_count++];
        alarm->hour = hour;
        alarm->minute = minute;
        alarm->repeat = s_repeat;
        alarm->custom_days = s_custom_days;
        alarm->enabled = true;
    }

    sort_alarms();
    rebuild_list();
    close_editor();
    note_activity();
}

static void delete_action_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (s_edit_index < 0 || s_edit_index >= s_alarm_count) return;

    for (int32_t i = s_edit_index; i < (int32_t)s_alarm_count - 1; ++i) {
        s_alarms[i] = s_alarms[i + 1];
    }
    if (s_alarm_count > 0) --s_alarm_count;

    rebuild_list();
    close_editor();
    note_activity();
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text,
                               int32_t w, int32_t h, lv_color_t bg,
                               lv_color_t fg, bool latin)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 9, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *label = latin
        ? make_latin_label(btn, text, fg, &lv_font_montserrat_16)
        : make_label(btn, text, fg);
    lv_obj_center(label);
    return btn;
}

static void reset_day_buttons(void)
{
    for (int i = 0; i < 7; ++i) {
        bool selected = (s_custom_days & (1u << i)) != 0;
        lv_obj_set_style_bg_color(s_day_buttons[i],
                                  selected ? UI_COLOR_ACCENT : UI_COLOR_PANEL_2, 0);
        lv_obj_set_style_text_color(s_day_buttons[i],
                                    selected ? UI_COLOR_BG : UI_COLOR_FG, 0);
    }
    set_day_error(false);
}

static void configure_editor_actions(bool editing)
{
    lv_label_set_text(s_primary_label, editing ? "Save" : "Add");
    lv_obj_set_width(s_primary_button, editing ? UI_EDIT_BUTTON_W : UI_RIGHT_W);
    lv_obj_set_x(s_primary_button,
                 editing ? UI_RIGHT_X + UI_EDIT_BUTTON_W + UI_EDIT_BUTTON_GAP
                         : UI_RIGHT_X);

    if (editing) {
        lv_obj_set_x(s_delete_button, UI_RIGHT_X);
        lv_obj_clear_flag(s_delete_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_delete_button, LV_OBJ_FLAG_HIDDEN);
    }
}

static void open_add_editor(lv_event_t *e)
{
    if (e != NULL && lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    s_edit_index = -1;
    s_repeat = UI_ALARM_REPEAT_NEVER;
    s_custom_days = 0;
    lv_roller_set_selected(s_hour_roller, 7, LV_ANIM_OFF);
    lv_roller_set_selected(s_minute_roller, 0, LV_ANIM_OFF);
    reset_day_buttons();
    refresh_repeat_buttons();
    set_time_error(false);
    configure_editor_actions(false);
    lv_obj_clear_flag(s_editor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_editor);
    note_activity();
}

static void open_editor_for_index(int32_t index)
{
    if (index < 0 || index >= s_alarm_count) return;

    s_edit_index = index;
    const ui_alarm_item_t *alarm = &s_alarms[index];
    s_repeat = alarm->repeat;
    s_custom_days = alarm->custom_days;
    lv_roller_set_selected(s_hour_roller, alarm->hour, LV_ANIM_OFF);
    lv_roller_set_selected(s_minute_roller, alarm->minute, LV_ANIM_OFF);
    reset_day_buttons();
    refresh_repeat_buttons();
    set_time_error(false);
    configure_editor_actions(true);
    lv_obj_clear_flag(s_editor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_editor);
    note_activity();
}

static void style_roller(lv_obj_t *roller)
{
    lv_obj_set_size(roller, UI_ROLLER_W, UI_ROLLER_H);
    lv_obj_set_style_bg_color(roller, UI_COLOR_PANEL_2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, UI_COLOR_ACCENT, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, UI_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, UI_COLOR_BG, LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_28, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(roller, UI_COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_border_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(roller, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(roller, roller_value_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void build_editor(lv_obj_t *parent)
{
    s_editor = plain_obj(parent);
    lv_obj_set_size(s_editor, UI_CONTENT_W, UI_SCREEN_H);
    lv_obj_set_pos(s_editor, UI_BACK_RAIL_W, 0);
    lv_obj_set_style_bg_color(s_editor, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_editor, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_editor, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *panel = plain_obj(s_editor);
    lv_obj_set_size(panel, UI_EDITOR_PANEL_W, UI_EDITOR_PANEL_H);
    lv_obj_set_pos(panel, UI_EDITOR_PANEL_X, UI_EDITOR_PANEL_Y);
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
    style_roller(s_hour_roller);
    lv_obj_set_pos(s_hour_roller, UI_ROLLER_1_X, 6);

    s_minute_roller = lv_roller_create(panel);
    lv_roller_set_options(s_minute_roller, minutes, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(s_minute_roller, 3);
    style_roller(s_minute_roller);
    lv_obj_set_pos(s_minute_roller, UI_ROLLER_2_X, 6);

    lv_obj_t *repeat_label = make_label(panel, "重复", UI_COLOR_MUTED);
    lv_obj_set_pos(repeat_label, UI_RIGHT_X, 5);

    const char *repeat_names[3] = {"永不", "每天", "自定义"};
    const int repeat_w[3] = {66, 66, 72};
    int32_t repeat_x = UI_RIGHT_X;
    for (int i = 0; i < 3; ++i) {
        s_repeat_buttons[i] = create_button(panel, repeat_names[i], repeat_w[i], 30,
                                            UI_COLOR_PANEL_2, UI_COLOR_FG, false);
        lv_obj_set_pos(s_repeat_buttons[i], repeat_x, 28);
        lv_obj_add_event_cb(s_repeat_buttons[i], repeat_button_event_cb,
                            LV_EVENT_CLICKED, (void *)(intptr_t)i);
        repeat_x += repeat_w[i] + 5;
    }

    s_days_row = plain_obj(panel);
    lv_obj_set_size(s_days_row, UI_RIGHT_W, 30);
    lv_obj_set_pos(s_days_row, UI_RIGHT_X, 65);
    const char *day_names[7] = {"一", "二", "三", "四", "五", "六", "日"};
    for (int i = 0; i < 7; ++i) {
        s_day_buttons[i] = create_button(s_days_row, day_names[i], 26, 28,
                                         UI_COLOR_PANEL_2, UI_COLOR_FG, false);
        lv_obj_set_pos(s_day_buttons[i], i * 30, 0);
        lv_obj_add_event_cb(s_day_buttons[i], day_button_event_cb,
                            LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *divider = plain_obj(panel);
    lv_obj_set_size(divider, UI_RIGHT_W, 1);
    lv_obj_set_pos(divider, UI_RIGHT_X, 103);
    lv_obj_set_style_bg_color(divider, UI_COLOR_LINE, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

    s_primary_button = create_button(panel, "Add", UI_RIGHT_W, 42,
                                     UI_COLOR_ACCENT, UI_COLOR_BG, true);
    lv_obj_set_pos(s_primary_button, UI_RIGHT_X, 114);
    s_primary_label = lv_obj_get_child(s_primary_button, 0);
    lv_obj_add_event_cb(s_primary_button, primary_action_event_cb,
                        LV_EVENT_CLICKED, NULL);

    s_delete_button = create_button(panel, "Del", UI_EDIT_BUTTON_W, 42,
                                    UI_COLOR_PANEL_2, UI_COLOR_DANGER, true);
    lv_obj_set_pos(s_delete_button, UI_RIGHT_X, 114);
    lv_obj_set_style_border_width(s_delete_button, 1, 0);
    lv_obj_set_style_border_color(s_delete_button, UI_COLOR_DANGER, 0);
    lv_obj_set_style_border_opa(s_delete_button, LV_OPA_70, 0);
    lv_obj_add_event_cb(s_delete_button, delete_action_event_cb,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_delete_button, LV_OBJ_FLAG_HIDDEN);

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
    lv_obj_set_style_pad_top(s_list, 4, 0);
    lv_obj_set_style_pad_bottom(s_list, 4, 0);
    lv_obj_set_style_pad_row(s_list, 6, 0);

    lv_obj_t *add_card = plain_obj(s_root);
    lv_obj_set_size(add_card, UI_ADD_CARD_W, UI_ADD_CARD_H);
    lv_obj_set_pos(add_card,
                   UI_BACK_RAIL_W + (UI_CONTENT_W - UI_ADD_CARD_W) / 2,
                   UI_LIST_H);
    lv_obj_set_style_bg_color(add_card, UI_COLOR_TAB, 0);
    lv_obj_set_style_bg_opa(add_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(add_card, 12, 0);
    lv_obj_add_flag(add_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(add_card, open_add_editor, LV_EVENT_CLICKED, NULL);

    lv_obj_t *plus = make_latin_label(add_card, "+", UI_COLOR_ACCENT,
                                      &lv_font_montserrat_28);
    lv_obj_center(plus);

    build_editor(s_root);
    rebuild_list();
    return s_root;
}

void ui_page_alarm_stop(void)
{
    if (s_root != NULL) lv_obj_delete(s_root);

    s_root = NULL;
    s_list = NULL;
    s_editor = NULL;
    s_hour_roller = NULL;
    s_minute_roller = NULL;
    s_days_row = NULL;
    s_primary_button = NULL;
    s_primary_label = NULL;
    s_delete_button = NULL;
    memset(s_repeat_buttons, 0, sizeof(s_repeat_buttons));
    memset(s_day_buttons, 0, sizeof(s_day_buttons));
    s_edit_index = -1;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}

bool ui_page_alarm_editor_active(void)
{
    return s_editor != NULL && !lv_obj_has_flag(s_editor, LV_OBJ_FLAG_HIDDEN);
}

void ui_page_alarm_close_editor(void)
{
    if (!ui_page_alarm_editor_active()) return;
    close_editor();
    note_activity();
}
