#include "ui_page_timer.h"

#include <stdbool.h>
#include <stdint.h>

#define TIMER_SCREEN_W              640
#define TIMER_SCREEN_H              172
#define TIMER_CONTENT_X              56
#define TIMER_CONTENT_W             (TIMER_SCREEN_W - TIMER_CONTENT_X)
#define TIMER_GROUP_W               118
#define TIMER_GROUP_GAP              10
#define TIMER_GROUP_X0                4
#define TIMER_ACTION_X              396
#define TIMER_ACTION_W              (TIMER_CONTENT_W - TIMER_ACTION_X)
#define TIMER_DRAG_STEP_PX           32
#define TIMER_MAX_SECONDS         43200u
#define TIMER_TICK_PERIOD_MS         100u
#define TIMER_QUICK_SWIPE_MS         260u
#define TIMER_QUICK_SWIPE_PX          26

#define TIMER_BG                    lv_color_hex(0x000000)
#define TIMER_FG                    lv_color_hex(0xFFFFFF)
#define TIMER_GREEN                 lv_color_hex(0x38D66B)
#define TIMER_ORANGE                lv_color_hex(0xFF9F2F)
#define TIMER_RED                   lv_color_hex(0xFF4D5E)

typedef enum {
    TIMER_STATE_SETTING = 0,
    TIMER_STATE_RUNNING,
    TIMER_STATE_PAUSED,
    TIMER_STATE_FINISHED,
} timer_state_t;

typedef struct {
    lv_obj_t *seg[7];
} seven_digit_t;

typedef struct {
    lv_obj_t *zone;
    seven_digit_t digits[2];
    uint8_t kind;
    int16_t press_y;
    uint8_t start_value;
    uint32_t press_tick;
    bool pressed;
} timer_column_t;

static timer_state_t s_state = TIMER_STATE_SETTING;
static uint32_t s_config_seconds = 0;
static uint32_t s_remaining_seconds = 0;
static uint32_t s_deadline_tick = 0;
static lv_timer_t *s_tick_timer = NULL;

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_action_area = NULL;
static lv_obj_t *s_primary_button = NULL;
static lv_obj_t *s_secondary_button = NULL;
static timer_column_t s_columns[3];

static ui_timer_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;

static const uint8_t DIGIT_MASKS[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

static void note_activity(void)
{
    if (s_activity_cb != NULL) {
        s_activity_cb(s_activity_user_data);
    }
}

static uint8_t clamp_u8(int value, int lo, int hi)
{
    if (value < lo) return (uint8_t)lo;
    if (value > hi) return (uint8_t)hi;
    return (uint8_t)value;
}

static void split_seconds(uint32_t total,
                          uint8_t *hours,
                          uint8_t *minutes,
                          uint8_t *seconds)
{
    if (total > TIMER_MAX_SECONDS) total = TIMER_MAX_SECONDS;
    *hours = (uint8_t)(total / 3600u);
    total %= 3600u;
    *minutes = (uint8_t)(total / 60u);
    *seconds = (uint8_t)(total % 60u);
}

static uint32_t combine_seconds(uint8_t hours,
                                uint8_t minutes,
                                uint8_t seconds)
{
    uint32_t total = (uint32_t)hours * 3600u +
                     (uint32_t)minutes * 60u +
                     (uint32_t)seconds;
    return total > TIMER_MAX_SECONDS ? TIMER_MAX_SECONDS : total;
}

static void set_segment(lv_obj_t *obj, bool on)
{
    if (obj == NULL) return;
    lv_obj_set_style_bg_color(obj, TIMER_FG, 0);
    lv_obj_set_style_bg_opa(obj, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

static lv_obj_t *make_segment(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *seg = lv_obj_create(parent);
    lv_obj_remove_style_all(seg);
    lv_obj_set_pos(seg, x, y);
    lv_obj_set_size(seg, w, h);
    lv_obj_set_style_radius(seg, 3, 0);
    lv_obj_set_style_bg_opa(seg, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(seg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
    return seg;
}

static void build_digit(lv_obj_t *parent, seven_digit_t *digit, int x)
{
    lv_obj_t *holder = lv_obj_create(parent);
    lv_obj_remove_style_all(holder);
    lv_obj_set_pos(holder, x, 0);
    lv_obj_set_size(holder, 52, 124);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_SCROLLABLE);

    digit->seg[0] = make_segment(holder, 10,   2, 32, 8);
    digit->seg[1] = make_segment(holder, 42,  11,  8, 45);
    digit->seg[2] = make_segment(holder, 42,  67,  8, 45);
    digit->seg[3] = make_segment(holder, 10, 114, 32, 8);
    digit->seg[4] = make_segment(holder,  2,  67,  8, 45);
    digit->seg[5] = make_segment(holder,  2,  11,  8, 45);
    digit->seg[6] = make_segment(holder, 10,  58, 32, 8);
}

static void digit_set(seven_digit_t *digit, uint8_t value)
{
    uint8_t mask = value < 10 ? DIGIT_MASKS[value] : 0;
    for (int i = 0; i < 7; ++i) {
        set_segment(digit->seg[i], (mask & (1u << i)) != 0);
    }
}

static void column_set_value(timer_column_t *column, uint8_t value)
{
    digit_set(&column->digits[0], (uint8_t)(value / 10u));
    digit_set(&column->digits[1], (uint8_t)(value % 10u));
}

static uint8_t column_value(uint8_t kind)
{
    uint8_t h, m, s;
    uint32_t total = s_state == TIMER_STATE_SETTING
                         ? s_config_seconds
                         : s_remaining_seconds;
    split_seconds(total, &h, &m, &s);
    if (kind == 0) return h;
    if (kind == 1) return m;
    return s;
}

static void refresh_columns(void)
{
    for (int i = 0; i < 3; ++i) {
        column_set_value(&s_columns[i], column_value((uint8_t)i));
    }
}

static void set_columns_enabled(bool enabled)
{
    for (int i = 0; i < 3; ++i) {
        if (s_columns[i].zone == NULL) continue;
        if (enabled) {
            lv_obj_add_flag(s_columns[i].zone, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_clear_flag(s_columns[i].zone, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

static lv_obj_t *make_round_button(lv_obj_t *parent,
                                   int size,
                                   lv_color_t color,
                                   const char *symbol)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, size, size);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_30, 0);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_label_create(button);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, color, 0);
    lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0);
    lv_obj_center(icon);
    return button;
}

static void action_primary_cb(lv_event_t *e);
static void action_secondary_cb(lv_event_t *e);

static void rebuild_action_area(void)
{
    if (s_action_area == NULL) return;

    lv_obj_clean(s_action_area);
    s_primary_button = NULL;
    s_secondary_button = NULL;

    if (s_state == TIMER_STATE_SETTING) {
        s_primary_button = make_round_button(s_action_area,
                                             92,
                                             TIMER_GREEN,
                                             LV_SYMBOL_PLAY);
        lv_obj_center(s_primary_button);
        lv_obj_add_event_cb(s_primary_button,
                            action_primary_cb,
                            LV_EVENT_CLICKED,
                            NULL);
        set_columns_enabled(true);
        return;
    }

    const char *primary_symbol = s_state == TIMER_STATE_RUNNING
                                     ? LV_SYMBOL_PAUSE
                                     : LV_SYMBOL_PLAY;

    s_primary_button = make_round_button(s_action_area,
                                         60,
                                         TIMER_ORANGE,
                                         primary_symbol);
    lv_obj_align(s_primary_button, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_add_event_cb(s_primary_button,
                        action_primary_cb,
                        LV_EVENT_CLICKED,
                        NULL);

    s_secondary_button = make_round_button(s_action_area,
                                           60,
                                           TIMER_RED,
                                           LV_SYMBOL_STOP);
    lv_obj_align(s_secondary_button, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_add_event_cb(s_secondary_button,
                        action_secondary_cb,
                        LV_EVENT_CLICKED,
                        NULL);
    set_columns_enabled(false);
}

static void stop_tick_timer(void)
{
    if (s_tick_timer != NULL) {
        lv_timer_delete(s_tick_timer);
        s_tick_timer = NULL;
    }
}

static void finish_countdown(void)
{
    stop_tick_timer();
    s_remaining_seconds = 0;
    s_state = TIMER_STATE_FINISHED;
    refresh_columns();
    rebuild_action_area();
    note_activity();
}

static void timer_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_state != TIMER_STATE_RUNNING) return;

    note_activity();

    uint32_t now = lv_tick_get();
    int32_t ms_left = (int32_t)(s_deadline_tick - now);
    if (ms_left <= 0) {
        finish_countdown();
        return;
    }

    uint32_t seconds_left = ((uint32_t)ms_left + 999u) / 1000u;
    if (seconds_left != s_remaining_seconds) {
        s_remaining_seconds = seconds_left;
        refresh_columns();
    }
}

static void begin_running(uint32_t seconds)
{
    if (seconds == 0 || seconds > TIMER_MAX_SECONDS) return;

    stop_tick_timer();
    s_remaining_seconds = seconds;
    s_deadline_tick = lv_tick_get() + seconds * 1000u;
    s_state = TIMER_STATE_RUNNING;
    s_tick_timer = lv_timer_create(timer_tick_cb,
                                   TIMER_TICK_PERIOD_MS,
                                   NULL);
    refresh_columns();
    rebuild_action_area();
    note_activity();
}

static void pause_running(void)
{
    if (s_state != TIMER_STATE_RUNNING) return;

    uint32_t now = lv_tick_get();
    int32_t ms_left = (int32_t)(s_deadline_tick - now);
    s_remaining_seconds = ms_left > 0
                              ? ((uint32_t)ms_left + 999u) / 1000u
                              : 0;
    stop_tick_timer();

    s_state = s_remaining_seconds == 0
                  ? TIMER_STATE_FINISHED
                  : TIMER_STATE_PAUSED;
    refresh_columns();
    rebuild_action_area();
}

static void restore_setting(void)
{
    stop_tick_timer();
    s_state = TIMER_STATE_SETTING;
    s_remaining_seconds = s_config_seconds;
    refresh_columns();
    rebuild_action_area();
}

static void action_primary_cb(lv_event_t *e)
{
    (void)e;
    note_activity();

    if (s_state == TIMER_STATE_SETTING) {
        begin_running(s_config_seconds);
    } else if (s_state == TIMER_STATE_RUNNING) {
        pause_running();
    } else if (s_state == TIMER_STATE_PAUSED) {
        begin_running(s_remaining_seconds);
    } else if (s_state == TIMER_STATE_FINISHED) {
        begin_running(s_config_seconds);
    }
}

static void action_secondary_cb(lv_event_t *e)
{
    (void)e;
    note_activity();
    restore_setting();
}

static void update_setting_from_column(timer_column_t *column, int value)
{
    if (s_state != TIMER_STATE_SETTING || column == NULL) return;

    uint8_t h, m, s;
    split_seconds(s_config_seconds, &h, &m, &s);

    if (column->kind == 0) h = clamp_u8(value, 0, 12);
    else if (column->kind == 1) m = clamp_u8(value, 0, 59);
    else s = clamp_u8(value, 0, 59);

    if (h >= 12) {
        h = 12;
        m = 0;
        s = 0;
    }

    s_config_seconds = combine_seconds(h, m, s);
    s_remaining_seconds = s_config_seconds;
    refresh_columns();
}

static void column_event_cb(lv_event_t *e)
{
    timer_column_t *column = (timer_column_t *)lv_event_get_user_data(e);
    if (column == NULL || s_state != TIMER_STATE_SETTING) return;

    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        column->pressed = true;
        column->press_y = (int16_t)point.y;
        column->start_value = column_value(column->kind);
        column->press_tick = lv_tick_get();
        note_activity();
        return;
    }

    if (!column->pressed) return;

    if (code == LV_EVENT_PRESSING) {
        int delta = column->press_y - point.y;
        int steps = delta / TIMER_DRAG_STEP_PX;
        int max_value = column->kind == 0 ? 12 : 59;
        update_setting_from_column(
            column,
            clamp_u8((int)column->start_value + steps, 0, max_value));
        note_activity();
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (code == LV_EVENT_RELEASED) {
            const int delta = column->press_y - point.y;
            const uint32_t elapsed = lv_tick_get() - column->press_tick;
            if (elapsed <= TIMER_QUICK_SWIPE_MS &&
                (delta >= TIMER_QUICK_SWIPE_PX || delta <= -TIMER_QUICK_SWIPE_PX)) {
                const int max_value = column->kind == 0 ? 12 : 59;
                const int target = (int)column->start_value + (delta > 0 ? 1 : -1);
                update_setting_from_column(column, clamp_u8(target, 0, max_value));
            }
        }
        column->pressed = false;
        note_activity();
    }
}

static void build_column(lv_obj_t *parent,
                         timer_column_t *column,
                         uint8_t kind,
                         int x)
{
    column->kind = kind;
    column->pressed = false;

    column->zone = lv_obj_create(parent);
    lv_obj_remove_style_all(column->zone);
    lv_obj_set_pos(column->zone, x, 0);
    lv_obj_set_size(column->zone, TIMER_GROUP_W, TIMER_SCREEN_H);
    lv_obj_add_flag(column->zone, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(column->zone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *digits = lv_obj_create(column->zone);
    lv_obj_remove_style_all(digits);
    lv_obj_set_size(digits, 112, 124);
    lv_obj_align(digits, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(digits, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(digits, LV_OBJ_FLAG_SCROLLABLE);

    build_digit(digits, &column->digits[0], 2);
    build_digit(digits, &column->digits[1], 58);

    lv_obj_add_event_cb(column->zone,
                        column_event_cb,
                        LV_EVENT_PRESSED,
                        column);
    lv_obj_add_event_cb(column->zone,
                        column_event_cb,
                        LV_EVENT_PRESSING,
                        column);
    lv_obj_add_event_cb(column->zone,
                        column_event_cb,
                        LV_EVENT_RELEASED,
                        column);
    lv_obj_add_event_cb(column->zone,
                        column_event_cb,
                        LV_EVENT_PRESS_LOST,
                        column);
}

static void build_colon(lv_obj_t *parent, int x)
{
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *dot = lv_obj_create(parent);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_pos(dot, x, i == 0 ? 64 : 100);
        lv_obj_set_style_bg_color(dot, TIMER_FG, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }
}

void ui_page_timer_build(lv_obj_t *parent,
                         ui_timer_activity_cb_t activity_cb,
                         void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_pos(s_root, TIMER_CONTENT_X, 0);
    lv_obj_set_size(s_root, TIMER_CONTENT_W, TIMER_SCREEN_H);
    lv_obj_set_style_bg_color(s_root, TIMER_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    int x0 = TIMER_GROUP_X0;
    int x1 = x0 + TIMER_GROUP_W + TIMER_GROUP_GAP;
    int x2 = x1 + TIMER_GROUP_W + TIMER_GROUP_GAP;

    build_column(s_root, &s_columns[0], 0, x0);
    build_column(s_root, &s_columns[1], 1, x1);
    build_column(s_root, &s_columns[2], 2, x2);

    build_colon(s_root, x0 + TIMER_GROUP_W + 1);
    build_colon(s_root, x1 + TIMER_GROUP_W + 1);

    s_action_area = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_action_area);
    lv_obj_set_pos(s_action_area, TIMER_ACTION_X, 0);
    lv_obj_set_size(s_action_area, TIMER_ACTION_W, TIMER_SCREEN_H);
    lv_obj_clear_flag(s_action_area, LV_OBJ_FLAG_SCROLLABLE);

    if (s_state == TIMER_STATE_SETTING) {
        s_remaining_seconds = s_config_seconds;
    }

    refresh_columns();
    rebuild_action_area();
}

void ui_page_timer_stop(void)
{
    s_root = NULL;
    s_action_area = NULL;
    s_primary_button = NULL;
    s_secondary_button = NULL;

    for (int i = 0; i < 3; ++i) {
        s_columns[i].zone = NULL;
        s_columns[i].pressed = false;
        for (int d = 0; d < 2; ++d) {
            for (int seg = 0; seg < 7; ++seg) {
                s_columns[i].digits[d].seg[seg] = NULL;
            }
        }
    }

    if (s_state != TIMER_STATE_RUNNING) {
        s_activity_cb = NULL;
        s_activity_user_data = NULL;
    }
}

bool ui_page_timer_is_running(void)
{
    return s_state == TIMER_STATE_RUNNING;
}
