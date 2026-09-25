#include "ui_page_standby.h"
#include "ui_page_clock.h"
#include "ui_page_weather.h"
#include "ui_page_calendar.h"

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172
#define CLOCK_BG             lv_color_hex(0x000000)
#define CLOCK_HINT           lv_color_hex(0x777777)

#define TAP_MAX_DISTANCE     14
#define SWIPE_MIN_DISTANCE   36

static lv_timer_t *s_hint_timer = NULL;
static lv_obj_t *s_hint_left = NULL;
static lv_obj_t *s_hint_right = NULL;
static ui_standby_event_cb_t s_event_cb = NULL;
static void *s_event_user_data = NULL;
static lv_point_t s_press_point = {0, 0};
static bool s_press_valid = false;

static int32_t iabs32(int32_t value)
{
    return value < 0 ? -value : value;
}

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
    s_press_valid = false;
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

static void dispatch_release_gesture(lv_point_t release_point)
{
    if (!s_press_valid || s_event_cb == NULL) {
        return;
    }

    int32_t dx = release_point.x - s_press_point.x;
    int32_t dy = release_point.y - s_press_point.y;
    int32_t ax = iabs32(dx);
    int32_t ay = iabs32(dy);

    /*
     * Classify the complete press/release path ourselves instead of relying
     * on LV_EVENT_SHORT_CLICKED + LV_EVENT_GESTURE.  On a non-scrollable
     * standby root LVGL can still emit a short-click after a drag, which made
     * horizontal swipes accidentally open HOME.
     */
    if (ax <= TAP_MAX_DISTANCE && ay <= TAP_MAX_DISTANCE) {
        s_event_cb(UI_STANDBY_EVENT_OPEN_HOME, s_event_user_data);
        return;
    }

    if (ax >= SWIPE_MIN_DISTANCE && ax > ay) {
        if (dx < 0) {
            s_event_cb(UI_STANDBY_EVENT_NEXT, s_event_user_data);
        }
        else {
            s_event_cb(UI_STANDBY_EVENT_PREVIOUS, s_event_user_data);
        }
        return;
    }

    if (ay >= SWIPE_MIN_DISTANCE && ay > ax && dy < 0) {
        s_event_cb(UI_STANDBY_EVENT_OPEN_HOME, s_event_user_data);
    }
}

static void standby_input_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_press_point);
        s_press_valid = true;
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        lv_point_t release_point;
        lv_indev_get_point(indev, &release_point);
        dispatch_release_gesture(release_point);
        s_press_valid = false;
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        s_press_valid = false;
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
    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_PRESS_LOST, NULL);

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

    /* No persistent page dots. The < and > edge hints are shown only for
     * the first second after entering standby. */
    add_swipe_hint(root, show_swipe_hint);
}
