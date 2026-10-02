#include "ui_page_standby.h"
#include "ui_page_clock.h"

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172
#define CLOCK_BG             lv_color_hex(0x000000)

#define TAP_MAX_DISTANCE             14
#define VERTICAL_DRAG_START_DISTANCE 12

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

lv_obj_t *ui_page_standby_get_root(void)
{
    return s_root;
}

void ui_page_standby_stop(void)
{
    ui_page_clock_stop();

    s_root = NULL;
    s_press_valid = false;
    s_vertical_consumed = false;
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

    if (ax <= TAP_MAX_DISTANCE && ay <= TAP_MAX_DISTANCE) {
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
        s_vertical_consumed = false;
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
    (void)view;
    (void)show_swipe_hint;

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

    ui_page_clock_build(content);
}
