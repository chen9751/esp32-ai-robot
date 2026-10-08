#include "ui_page_feature.h"
#include "ui_page_remote.h"
#include "ui_page_lights.h"
#include "ui_page_music.h"
#include "ui_page_devices.h"
#include "ui_assets.h"

#include <stdint.h>

#define UI_SCREEN_W                    640
#define UI_SCREEN_H                    172
#define UI_BACK_RAIL_W                  56
#define UI_BACK_INDICATOR_W              4
#define UI_BACK_INDICATOR_H             36
#define UI_BACK_INDICATOR_X             20
#define UI_BACK_TAP_SLOP                10
#define UI_BACK_LOCK_DISTANCE            8
#define UI_BACK_COMMIT_DISTANCE         90
#define UI_BACK_ANIM_MS                180
#define UI_ENTER_OFFSET_PX              UI_SCREEN_W
#define UI_ENTER_ANIM_MS               UI_BACK_ANIM_MS

#define UI_COLOR_BG             lv_color_hex(0x000000)
#define UI_COLOR_FG             lv_color_hex(0xFFFFFF)
#define UI_COLOR_BACK_IDLE      lv_color_hex(0x6F6F73)
#define UI_COLOR_BACK_PRESSED   lv_color_hex(0xFFFFFF)

static lv_obj_t *s_content = NULL;
static lv_obj_t *s_rail = NULL;
static lv_obj_t *s_indicator = NULL;
static lv_point_t s_press = {0, 0};
static bool s_pressed = false;
static bool s_horizontal_drag = false;
static bool s_animating = false;
static ui_menu_action_t s_action = UI_MENU_REMOTE;
static ui_feature_back_cb_t s_back_cb = NULL;
static void *s_back_user_data = NULL;
static ui_feature_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;

static int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void note_activity(void)
{
    if (s_activity_cb != NULL) s_activity_cb(s_activity_user_data);
}

static void set_indicator_pressed(bool pressed)
{
    if (s_indicator == NULL) return;
    lv_obj_set_style_bg_color(s_indicator,
                              pressed ? UI_COLOR_BACK_PRESSED : UI_COLOR_BACK_IDLE,
                              0);
}

static void content_set_x(void *obj, int32_t x) { lv_obj_set_x((lv_obj_t *)obj, x); }

static void finish_back(lv_anim_t *anim)
{
    (void)anim;
    s_animating = false;
    if (s_back_cb != NULL) s_back_cb(s_back_user_data);
}

static void finish_rest(lv_anim_t *anim)
{
    (void)anim;
    s_animating = false;
    s_horizontal_drag = false;
}

static void animate_content_to(int32_t end_x, uint32_t duration_ms, bool commit)
{
    if (s_content == NULL || s_animating) return;
    s_animating = true;

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_content);
    lv_anim_set_values(&anim, lv_obj_get_x(s_content), end_x);
    lv_anim_set_duration(&anim, duration_ms);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, content_set_x);
    lv_anim_set_completed_cb(&anim, commit ? finish_back : finish_rest);
    lv_anim_start(&anim);
}

static void back_rail_event_cb(lv_event_t *e)
{
    if (s_content == NULL || s_animating) return;
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_press);
        s_pressed = true;
        s_horizontal_drag = false;
        set_indicator_pressed(true);
        note_activity();
        return;
    }
    if (!s_pressed) return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    int32_t dx = point.x - s_press.x;
    int32_t dy = point.y - s_press.y;
    int32_t ax = iabs32(dx);
    int32_t ay = iabs32(dy);

    if (code == LV_EVENT_PRESSING) {
        note_activity();
        if (!s_horizontal_drag && ax >= UI_BACK_LOCK_DISTANCE && dx > 0 && ax > ay) {
            s_horizontal_drag = true;
        }
        if (s_horizontal_drag) lv_obj_set_x(s_content, clamp_i32(dx, 0, UI_SCREEN_W));
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        s_pressed = false;
        set_indicator_pressed(false);
        note_activity();
        if (s_horizontal_drag) {
            animate_content_to(dx >= UI_BACK_COMMIT_DISTANCE ? UI_SCREEN_W : 0,
                               UI_BACK_ANIM_MS,
                               dx >= UI_BACK_COMMIT_DISTANCE);
        } else if (ax <= UI_BACK_TAP_SLOP && ay <= UI_BACK_TAP_SLOP) {
            animate_content_to(UI_SCREEN_W, UI_BACK_ANIM_MS, true);
        }
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        s_pressed = false;
        set_indicator_pressed(false);
        if (s_horizontal_drag || lv_obj_get_x(s_content) != 0) {
            animate_content_to(0, UI_BACK_ANIM_MS, false);
        }
        s_horizontal_drag = false;
    }
}

lv_obj_t *ui_page_feature_build(lv_obj_t *parent,
                                ui_menu_action_t action,
                                ui_feature_back_cb_t back_cb,
                                void *back_user_data,
                                ui_feature_activity_cb_t activity_cb,
                                void *activity_user_data)
{
    s_action = action;
    s_back_cb = back_cb;
    s_back_user_data = back_user_data;
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_pressed = false;
    s_horizontal_drag = false;
    s_animating = false;

    s_content = lv_obj_create(parent);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_size(s_content, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_content, UI_ENTER_OFFSET_PX, 0);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    if (action == UI_MENU_REMOTE) {
        ui_page_remote_build(s_content, s_activity_cb, s_activity_user_data);
    } else if (action == UI_MENU_LIGHTS) {
        ui_page_lights_build(s_content, s_activity_cb, s_activity_user_data);
    } else if (action == UI_MENU_MUSIC) {
        ui_page_music_build(s_content, s_activity_cb, s_activity_user_data);
    } else if (action == UI_MENU_DEVICES) {
        ui_page_devices_build(s_content, s_activity_cb, s_activity_user_data);
    }

    s_rail = lv_obj_create(s_content);
    lv_obj_remove_style_all(s_rail);
    lv_obj_set_size(s_rail, UI_BACK_RAIL_W, UI_SCREEN_H);
    lv_obj_set_pos(s_rail, 0, 0);
    lv_obj_set_style_bg_opa(s_rail, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_rail, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_rail, LV_OBJ_FLAG_SCROLLABLE);

    s_indicator = lv_obj_create(s_rail);
    lv_obj_remove_style_all(s_indicator);
    lv_obj_set_size(s_indicator, UI_BACK_INDICATOR_W, UI_BACK_INDICATOR_H);
    lv_obj_set_pos(s_indicator, UI_BACK_INDICATOR_X,
                   (UI_SCREEN_H - UI_BACK_INDICATOR_H) / 2);
    lv_obj_set_style_bg_color(s_indicator, UI_COLOR_BACK_IDLE, 0);
    lv_obj_set_style_bg_opa(s_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_indicator, UI_BACK_INDICATOR_W / 2, 0);
    lv_obj_clear_flag(s_indicator, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_indicator, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(s_rail, back_rail_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_rail, back_rail_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_rail, back_rail_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_rail, back_rail_event_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_move_foreground(s_rail);
    animate_content_to(0, UI_ENTER_ANIM_MS, false);
    return s_content;
}

void ui_page_feature_stop(void)
{
    if (s_content == NULL) return;
    if (s_action == UI_MENU_REMOTE) ui_page_remote_stop();
    if (s_action == UI_MENU_LIGHTS) ui_page_lights_stop();
    if (s_action == UI_MENU_MUSIC) ui_page_music_stop();
    if (s_action == UI_MENU_DEVICES) ui_page_devices_stop();

    if (s_content != NULL) {
        lv_anim_delete(s_content, NULL);
        lv_obj_delete(s_content);
    }

    s_content = NULL;
    s_rail = NULL;
    s_indicator = NULL;
    s_pressed = false;
    s_horizontal_drag = false;
    s_animating = false;
    s_action = UI_MENU_REMOTE;
    s_back_cb = NULL;
    s_back_user_data = NULL;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}

bool ui_page_feature_active(void)
{
    return s_content != NULL;
}
