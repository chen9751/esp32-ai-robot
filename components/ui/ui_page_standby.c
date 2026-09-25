#include "ui_page_standby.h"
#include "ui_page_clock.h"
#include "ui_page_weather.h"
#include "ui_page_calendar.h"

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172
#define CLOCK_BG             lv_color_hex(0x000000)
#define CLOCK_HINT           lv_color_hex(0x8A8A8A)
#define CLOCK_HINT_OPA       LV_OPA_40
#define CLOCK_HINT_WIDTH     2

#define TAP_MAX_DISTANCE             14
#define SWIPE_MIN_DISTANCE           36
#define VERTICAL_DRAG_START_DISTANCE 12

static lv_timer_t *s_hint_timer = NULL;
static lv_obj_t *s_hint_left = NULL;
static lv_obj_t *s_hint_right = NULL;
static lv_obj_t *s_root = NULL;
static ui_standby_event_cb_t s_event_cb = NULL;
static void *s_event_user_data = NULL;
static lv_point_t s_press_point = {0, 0};
static bool s_press_valid = false;
static bool s_vertical_consumed = false;
static ui_vertical_drag_cb_t s_vertical_drag_cb = NULL;
static void *s_vertical_drag_user_data = NULL;

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

lv_obj_t *ui_page_standby_get_root(void)
{
    return s_root;
}

void ui_page_standby_stop(void)
{
    delete_hint_timer();
    ui_page_clock_stop();

    s_hint_left = NULL;
    s_hint_right = NULL;
    s_root = NULL;
    s_press_valid = false;
    s_vertical_consumed = false;
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

static lv_obj_t *create_chevron(lv_obj_t *parent, bool points_right)
{
    static const lv_point_precise_t left_points[] = {
        {14, 0},
        {0, 22},
        {14, 44},
    };
    static const lv_point_precise_t right_points[] = {
        {0, 0},
        {14, 22},
        {0, 44},
    };

    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line,
                       points_right ? right_points : left_points,
                       3);
    lv_obj_set_size(line, 15, 45);
    lv_obj_set_style_line_width(line, CLOCK_HINT_WIDTH, 0);
    lv_obj_set_style_line_color(line, CLOCK_HINT, 0);
    lv_obj_set_style_line_opa(line, CLOCK_HINT_OPA, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

static void add_swipe_hint(lv_obj_t *parent, bool visible)
{
    /*
     * Draw the hint directly with LVGL lines instead of text glyphs.
     * This keeps it slimmer/taller, avoids a font dependency, and is cheaper
     * than adding another image/icon asset for two temporary chevrons.
     */
    s_hint_left = create_chevron(parent, false);
    lv_obj_align(s_hint_left, LV_ALIGN_LEFT_MID, 14, 0);

    s_hint_right = create_chevron(parent, true);
    lv_obj_align(s_hint_right, LV_ALIGN_RIGHT_MID, -14, 0);

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

    /*
     * Upward motion is handled by ui_manager.c as an interactive page
     * transition so the HOME page follows the finger.  Keep this controller
     * responsible only for taps and horizontal clock/weather/calendar paging.
     */
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
        s_vertical_consumed = false;

        /* Any real touch is activity, even if the user holds without
         * completing a tap/swipe before the 60-second idle deadline. */
        ui_mark_activity();
        return;
    }

    if (code == LV_EVENT_PRESSING && s_press_valid) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);

        int32_t dx = point.x - s_press_point.x;
        int32_t dy = point.y - s_press_point.y;

        if (s_vertical_drag_cb != NULL &&
            dy <= -VERTICAL_DRAG_START_DISTANCE &&
            iabs32(dy) > iabs32(dx)) {
            /*
             * Do not consume tiny upward finger jitter.  Without this
             * threshold a normal tap that drifted by even 1 px upward was
             * classified as a vertical gesture and the tap was lost.
             */
            s_vertical_consumed = true;
            s_vertical_drag_cb(dx,
                               dy,
                               false,
                               false,
                               s_vertical_drag_user_data);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        lv_point_t release_point;
        lv_indev_get_point(indev, &release_point);

        int32_t dx = release_point.x - s_press_point.x;
        int32_t dy = release_point.y - s_press_point.y;

        if (s_vertical_consumed && s_vertical_drag_cb != NULL) {
            s_vertical_drag_cb(dx,
                               dy,
                               true,
                               false,
                               s_vertical_drag_user_data);
        }
        else {
            dispatch_release_gesture(release_point);
        }

        s_press_valid = false;
        s_vertical_consumed = false;
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        if (s_vertical_consumed &&
            s_press_valid &&
            s_vertical_drag_cb != NULL) {
            lv_point_t point;
            lv_indev_get_point(indev, &point);

            s_vertical_drag_cb(point.x - s_press_point.x,
                               point.y - s_press_point.y,
                               true,
                               true,
                               s_vertical_drag_user_data);
        }

        s_press_valid = false;
        s_vertical_consumed = false;
    }
}

void ui_page_standby_show(ui_standby_view_t view,
                          bool show_swipe_hint,
                          ui_standby_event_cb_t event_cb,
                          void *event_user_data,
                          ui_vertical_drag_cb_t vertical_drag_cb,
                          void *vertical_drag_user_data)
{
    ui_page_standby_stop();

    s_event_cb = event_cb;
    s_event_user_data = event_user_data;
    s_vertical_drag_cb = vertical_drag_cb;
    s_vertical_drag_user_data = vertical_drag_user_data;

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_size(screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(screen, CLOCK_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *root = lv_obj_create(screen);
    s_root = root;
    lv_obj_null_on_delete(&s_root);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_center(root);
    lv_obj_set_style_bg_color(root, CLOCK_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_PRESSING, NULL);
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
