#include "ui_page_standby.h"
#include "ui_page_clock.h"
#include "ui_page_weather.h"
#include "ui_page_calendar.h"

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172
#define CLOCK_GREEN          lv_color_hex(0x39FF14)
#define CLOCK_BG             lv_color_hex(0x000000)
#define CLOCK_HINT           lv_color_hex(0x777777)
#define CLOCK_DOT_OFF        lv_color_hex(0x2A2A2A)

static lv_timer_t *s_hint_timer = NULL;
static lv_obj_t *s_hint_left = NULL;
static lv_obj_t *s_hint_right = NULL;
static ui_standby_event_cb_t s_event_cb = NULL;
static void *s_event_user_data = NULL;
static bool s_gesture_seen = false;

static void delete_hint_timer(void)
{
    if (s_hint_timer != NULL) {
        lv_timer_delete(s_hint_timer);
        s_hint_timer = NULL;
    }
}

void ui_page_standby_stop(void)
{
    delete_hint_timer();
    ui_page_clock_stop();

    s_hint_left = NULL;
    s_hint_right = NULL;
}

static void hide_hint_timer_cb(lv_timer_t *timer)
{
    if (s_hint_left != NULL) {
        lv_obj_add_flag(s_hint_left, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_hint_right != NULL) {
        lv_obj_add_flag(s_hint_right, LV_OBJ_FLAG_HIDDEN);
    }

    lv_timer_delete(timer);
    s_hint_timer = NULL;
}

static void add_page_dots(lv_obj_t *parent, ui_standby_view_t active)
{
    const int dot_size = 7;
    const int gap = 12;
    const int total = 3 * dot_size + 2 * gap;
    const int x0 = (UI_SCREEN_W - total) / 2;
    const int y = UI_SCREEN_H - 12;

    for (int i = 0; i < 3; ++i) {
        lv_obj_t *dot = lv_obj_create(parent);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, dot_size, dot_size);
        lv_obj_set_pos(dot, x0 + i * (dot_size + gap), y);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot,
                                  i == (int)active ? CLOCK_GREEN : CLOCK_DOT_OFF,
                                  0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }
}

static void add_swipe_hint(lv_obj_t *parent, bool visible)
{
    s_hint_left = lv_label_create(parent);
    lv_label_set_text(s_hint_left, "<");
    lv_obj_set_style_text_font(s_hint_left, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_hint_left, CLOCK_HINT, 0);
    lv_obj_align(s_hint_left, LV_ALIGN_LEFT_MID, 10, -3);
    lv_obj_clear_flag(s_hint_left, LV_OBJ_FLAG_CLICKABLE);

    s_hint_right = lv_label_create(parent);
    lv_label_set_text(s_hint_right, ">");
    lv_obj_set_style_text_font(s_hint_right, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_hint_right, CLOCK_HINT, 0);
    lv_obj_align(s_hint_right, LV_ALIGN_RIGHT_MID, -10, -3);
    lv_obj_clear_flag(s_hint_right, LV_OBJ_FLAG_CLICKABLE);

    if (!visible) {
        lv_obj_add_flag(s_hint_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_hint_right, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    s_hint_timer = lv_timer_create(hide_hint_timer_cb, 1000, NULL);
}

static void standby_input_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        s_gesture_seen = false;
        return;
    }

    if (code == LV_EVENT_GESTURE) {
        s_gesture_seen = true;
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

        if (s_event_cb == NULL) {
            return;
        }

        if (dir == LV_DIR_TOP) {
            s_event_cb(UI_STANDBY_EVENT_OPEN_HOME, s_event_user_data);
        }
        else if (dir == LV_DIR_LEFT) {
            s_event_cb(UI_STANDBY_EVENT_NEXT, s_event_user_data);
        }
        else if (dir == LV_DIR_RIGHT) {
            s_event_cb(UI_STANDBY_EVENT_PREVIOUS, s_event_user_data);
        }
        return;
    }

    if (code == LV_EVENT_SHORT_CLICKED &&
        !s_gesture_seen &&
        s_event_cb != NULL) {
        s_event_cb(UI_STANDBY_EVENT_OPEN_HOME, s_event_user_data);
    }
}

void ui_page_standby_show(ui_standby_view_t view,
                          bool show_swipe_hint,
                          ui_standby_event_cb_t event_cb,
                          void *event_user_data)
{
    ui_page_standby_stop();

    s_event_cb = event_cb;
    s_event_user_data = event_user_data;

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_size(screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(screen, CLOCK_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_center(root);
    lv_obj_set_style_bg_color(root, CLOCK_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_SHORT_CLICKED, NULL);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    if (view == UI_STANDBY_CLOCK) {
        ui_page_clock_build(content);
    }
    else if (view == UI_STANDBY_WEATHER) {
        ui_page_weather_build(content);
    }
    else {
        ui_page_calendar_build(content);
    }

    add_page_dots(root, view);
    add_swipe_hint(root, show_swipe_hint);
}
