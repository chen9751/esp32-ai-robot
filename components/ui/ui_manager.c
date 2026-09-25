#include "ui_manager.h"
#include "ui_page_home.h"
#include "ui_page_standby.h"
#include "ui_page_clock.h"
#include "ui_assets.h"

#include <stdint.h>

#define UI_SCREEN_W                    640
#define UI_SCREEN_H                    172
#define UI_IDLE_TIMEOUT_MS             60000u
#define UI_TRANSITION_LOCK_DISTANCE    12
#define UI_TRANSITION_COMMIT_DISTANCE  56
#define UI_TRANSITION_DURATION_MS      180

typedef enum {
    UI_TOP_STANDBY = 0,
    UI_TOP_HOME,
} ui_top_page_t;

typedef enum {
    UI_TRANSITION_NONE = 0,
    UI_TRANSITION_TO_CLOCK,
    UI_TRANSITION_TO_HOME,
} ui_transition_target_t;

static ui_menu_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static const lv_font_t *s_menu_font = NULL;

static ui_top_page_t s_top_page = UI_TOP_STANDBY;
static ui_standby_view_t s_standby_view = UI_STANDBY_CLOCK;
static uint32_t s_last_activity_tick = 0;
static lv_timer_t *s_idle_timer = NULL;

static bool s_transition_animating = false;
static ui_transition_target_t s_transition_target = UI_TRANSITION_NONE;
static lv_obj_t *s_transition_overlay = NULL;

static int32_t iabs32(int32_t value)
{
    return value < 0 ? -value : value;
}

static int32_t clamp_i32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void ui_mark_activity(void)
{
    s_last_activity_tick = lv_tick_get();
}

bool ui_navigation_transition_active(void)
{
    return s_transition_overlay != NULL || s_transition_animating;
}

static void page_activity_cb(void *user_data)
{
    (void)user_data;
    ui_mark_activity();
}

static void transition_overlay_set_y(void *obj, int32_t y)
{
    lv_obj_set_y((lv_obj_t *)obj, y);
}

static lv_obj_t *create_transition_surface(void)
{
    lv_obj_t *overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    return overlay;
}

static void vertical_drag_cb(int32_t dx,
                             int32_t dy,
                             bool released,
                             bool cancelled,
                             void *user_data);

static void begin_transition(ui_transition_target_t target)
{
    if (s_transition_overlay != NULL || s_transition_animating) {
        return;
    }

    s_transition_target = target;
    s_transition_overlay = create_transition_surface();

    if (target == UI_TRANSITION_TO_CLOCK) {
        lv_obj_set_pos(s_transition_overlay, 0, -UI_SCREEN_H);
        ui_page_clock_build(s_transition_overlay);
    }
    else {
        lv_obj_set_pos(s_transition_overlay, 0, UI_SCREEN_H);
        ui_page_home_build(s_transition_overlay,
                           s_action_cb,
                           s_action_user_data,
                           page_activity_cb,
                           NULL,
                           vertical_drag_cb,
                           NULL);
    }

    lv_obj_move_foreground(s_transition_overlay);
}

static void update_transition_position(int32_t dy)
{
    if (s_transition_overlay == NULL) {
        return;
    }

    if (s_transition_target == UI_TRANSITION_TO_CLOCK) {
        int32_t progress = clamp_i32(dy, 0, UI_SCREEN_H);
        lv_obj_set_y(s_transition_overlay, -UI_SCREEN_H + progress);
    }
    else {
        int32_t progress = clamp_i32(-dy, 0, UI_SCREEN_H);
        lv_obj_set_y(s_transition_overlay, UI_SCREEN_H - progress);
    }
}

static void finish_transition_commit(lv_anim_t *anim)
{
    (void)anim;

    s_transition_animating = false;
    s_transition_overlay = NULL;

    if (s_transition_target == UI_TRANSITION_TO_CLOCK) {
        s_transition_target = UI_TRANSITION_NONE;
        ui_show_standby_clock();
    }
    else if (s_transition_target == UI_TRANSITION_TO_HOME) {
        s_transition_target = UI_TRANSITION_NONE;
        ui_show_main_menu();
    }
}

static void finish_transition_cancel(lv_anim_t *anim)
{
    (void)anim;

    if (s_transition_target == UI_TRANSITION_TO_CLOCK) {
        ui_page_clock_stop();
    }

    if (s_transition_overlay != NULL) {
        lv_obj_delete(s_transition_overlay);
    }

    s_transition_overlay = NULL;
    s_transition_target = UI_TRANSITION_NONE;
    s_transition_animating = false;
}

static void animate_transition_to(int32_t end_y, bool commit)
{
    if (s_transition_overlay == NULL) {
        return;
    }

    s_transition_animating = true;

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_transition_overlay);
    lv_anim_set_values(&anim, lv_obj_get_y(s_transition_overlay), end_y);
    lv_anim_set_duration(&anim, UI_TRANSITION_DURATION_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, transition_overlay_set_y);
    lv_anim_set_completed_cb(&anim,
                             commit ? finish_transition_commit
                                    : finish_transition_cancel);
    lv_anim_start(&anim);
}

static void vertical_drag_cb(int32_t dx,
                             int32_t dy,
                             bool released,
                             bool cancelled,
                             void *user_data)
{
    (void)user_data;
    ui_mark_activity();

    int32_t ax = iabs32(dx);
    int32_t ay = iabs32(dy);

    if (s_transition_overlay == NULL && !s_transition_animating) {
        if (ay < UI_TRANSITION_LOCK_DISTANCE || ay <= ax) {
            return;
        }

        if (s_top_page == UI_TOP_HOME && dy > 0) {
            begin_transition(UI_TRANSITION_TO_CLOCK);
        }
        else if (s_top_page == UI_TOP_STANDBY && dy < 0) {
            begin_transition(UI_TRANSITION_TO_HOME);
        }
        else {
            return;
        }
    }

    if (s_transition_overlay == NULL || s_transition_animating) {
        return;
    }

    update_transition_position(dy);

    if (!released) {
        return;
    }

    int32_t distance =
        s_transition_target == UI_TRANSITION_TO_CLOCK ? dy : -dy;

    bool commit = !cancelled &&
                  distance >= UI_TRANSITION_COMMIT_DISTANCE;

    if (commit) {
        animate_transition_to(0, true);
    }
    else {
        int32_t offscreen_y =
            s_transition_target == UI_TRANSITION_TO_CLOCK
                ? -UI_SCREEN_H
                : UI_SCREEN_H;
        animate_transition_to(offscreen_y, false);
    }
}

static void standby_event_cb(ui_standby_event_t event, void *user_data)
{
    (void)user_data;
    ui_mark_activity();

    if (event == UI_STANDBY_EVENT_OPEN_HOME) {
        if (!ui_navigation_transition_active()) {
            ui_show_main_menu();
        }
        return;
    }

    if (ui_navigation_transition_active()) {
        return;
    }

    if (event == UI_STANDBY_EVENT_NEXT) {
        s_standby_view =
            (ui_standby_view_t)(((int)s_standby_view + 1) % UI_STANDBY_COUNT);
    }
    else if (event == UI_STANDBY_EVENT_PREVIOUS) {
        s_standby_view =
            (ui_standby_view_t)(((int)s_standby_view + UI_STANDBY_COUNT - 1) %
                                UI_STANDBY_COUNT);
    }
    else {
        return;
    }

    ui_page_standby_show(s_standby_view,
                         false,
                         standby_event_cb,
                         NULL,
                         vertical_drag_cb,
                         NULL);
}

static void idle_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (ui_navigation_transition_active()) {
        return;
    }

    uint32_t elapsed = lv_tick_get() - s_last_activity_tick;
    if (elapsed < UI_IDLE_TIMEOUT_MS) {
        return;
    }

    if (s_top_page == UI_TOP_STANDBY &&
        s_standby_view == UI_STANDBY_CLOCK) {
        return;
    }

    ui_show_standby_clock();
}

void ui_set_menu_action_cb(ui_menu_action_cb_t cb, void *user_data)
{
    s_action_cb = cb;
    s_action_user_data = user_data;
}

void ui_set_menu_font(const lv_font_t *font)
{
    s_menu_font = font;
    (void)s_menu_font;
}

void ui_show_main_menu(void)
{
    ui_page_standby_stop();
    s_top_page = UI_TOP_HOME;
    ui_mark_activity();

    ui_page_home_show(s_action_cb,
                      s_action_user_data,
                      page_activity_cb,
                      NULL,
                      vertical_drag_cb,
                      NULL);
}

void ui_show_standby_clock(void)
{
    s_top_page = UI_TOP_STANDBY;
    s_standby_view = UI_STANDBY_CLOCK;
    ui_mark_activity();

    ui_page_standby_show(UI_STANDBY_CLOCK,
                         true,
                         standby_event_cb,
                         NULL,
                         vertical_drag_cb,
                         NULL);
}

void ui_init(void)
{
    ui_assets_init();

    if (s_idle_timer == NULL) {
        s_idle_timer = lv_timer_create(idle_timer_cb, 1000, NULL);
    }

    ui_show_standby_clock();
}
