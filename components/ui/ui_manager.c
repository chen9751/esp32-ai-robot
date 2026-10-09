#include "ui_manager.h"
#include "ui_page_home.h"
#include "ui_page_standby.h"
#include "ui_page_clock.h"
#include "ui_page_feature.h"
#include "ui_page_settings.h"
#include "ui_assets.h"
#include "ui_page_robot_face.h"
#include <stdatomic.h>

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
    UI_TOP_FEATURE,
    UI_TOP_SETTINGS,
} ui_top_page_t;

typedef enum {
    UI_TRANSITION_NONE = 0,
    UI_TRANSITION_TO_CLOCK,
    UI_TRANSITION_TO_HOME,
    UI_TRANSITION_TO_SETTINGS,
    UI_TRANSITION_FROM_SETTINGS,
} ui_transition_target_t;

static ui_menu_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static const lv_font_t *s_menu_font = NULL;

static ui_top_page_t s_top_page = UI_TOP_STANDBY;
static ui_standby_view_t s_standby_view = UI_STANDBY_CLOCK;
static uint32_t s_last_activity_tick = 0;
static lv_timer_t *s_idle_timer = NULL;
static bool s_ui_locked = false;
static lv_obj_t *s_lock_overlay = NULL;
static lv_obj_t *s_voice_overlay = NULL;
static lv_timer_t *s_voice_timer = NULL;
static uint32_t s_voice_until = 0;
static atomic_bool s_voice_wake_pending = ATOMIC_VAR_INIT(false);
#define UI_VOICE_PREVIEW_MS 5000u
#define UI_BACK_UNLOCK_HOLD_MS 3000u

static lv_obj_t *s_settings_root = NULL;
static lv_point_t s_settings_press;
static bool s_settings_tracking = false;

static bool s_transition_animating = false;
static ui_transition_target_t s_transition_target = UI_TRANSITION_NONE;
static lv_obj_t *s_transition_overlay = NULL;
static lv_obj_t *s_transition_underlay = NULL;

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

/* ri-lock-line: System/lock-line.svg from Remix Icon, 24x24 silhouette.
 * Keep this tiny A8 mask self-contained and separate from font resources. */
static uint8_t s_lock_pixels[24 * 24];
static lv_image_dsc_t s_lock_image = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_A8,
               .w = 24, .h = 24, .stride = 24},
    .data_size = sizeof(s_lock_pixels), .data = s_lock_pixels
};

static void init_lock_icon(void)
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    /* Pixel-aligned coverage of the original Remix 24x24 lock path:
     * body x=3..20 y=10..21, cutout x=5..18 y=12..19;
     * shackle outer and inner are concentric radius 7 and 5 arches. */
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 24; ++x) {
            bool body = x >= 3 && x < 21 && y >= 10 && y < 22 &&
                        !(x >= 5 && x < 19 && y >= 12 && y < 20);
            bool keyhole = x >= 11 && x < 13 && y >= 14 && y < 18;
            int dx = 2 * x + 1 - 24;
            int dy = 2 * y + 1 - 18;
            bool arch = y >= 2 && y < 11 &&
                        (dx * dx + dy * dy <= 196) &&
                        (dx * dx + dy * dy >= 100 || y >= 9);
            s_lock_pixels[y * 24 + x] = (body || keyhole || arch) ? 255 : 0;
        }
    }
    s_lock_image.data = s_lock_pixels;
}

static void refresh_lock_overlay(void)
{
    if (s_lock_overlay != NULL) {
        lv_obj_delete(s_lock_overlay);
        s_lock_overlay = NULL;
    }
    if (!s_ui_locked) return;
    init_lock_icon();
    lv_obj_t *screen = lv_screen_active();
    s_lock_overlay = lv_obj_create(screen);
    lv_obj_null_on_delete(&s_lock_overlay);
    lv_obj_remove_style_all(s_lock_overlay);
    lv_obj_set_pos(s_lock_overlay, 0, 0);
    lv_obj_set_size(s_lock_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_add_flag(s_lock_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_lock_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_lock_overlay, LV_OPA_TRANSP, 0);
    lv_obj_t *icon = lv_image_create(s_lock_overlay);
    lv_image_set_src(icon, &s_lock_image);
    lv_obj_set_style_image_recolor(icon, lv_color_hex(0x9A9A9A), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_set_pos(icon, UI_SCREEN_W - 34, 10);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(s_lock_overlay);
}

bool ui_is_locked(void) { return s_ui_locked; }

void ui_unlock_from_back_hold(void)
{
    if (!s_ui_locked) return;
    s_ui_locked = false;
    refresh_lock_overlay();
    ui_mark_activity();
}

/* Called by the WakeNet worker (not the LVGL task). Never touch LVGL here. */
void ui_notify_voice_wakeup(void)
{
    atomic_store_explicit(&s_voice_wake_pending, true, memory_order_release);
}

static void voice_overlay_close(void)
{
    ui_page_robot_face_stop();
    if (s_voice_overlay != NULL) {
        lv_obj_delete(s_voice_overlay);
        s_voice_overlay = NULL;
    }
    s_voice_until = 0;
}

static void voice_overlay_show(void)
{
    if (ui_navigation_transition_active()) {
        /* Avoid overlaying a screen that is still being animated. */
        return;
    }
    if (s_voice_overlay == NULL) {
        s_voice_overlay = lv_obj_create(lv_screen_active());
        lv_obj_null_on_delete(&s_voice_overlay);
        lv_obj_remove_style_all(s_voice_overlay);
        lv_obj_set_size(s_voice_overlay, UI_SCREEN_W, UI_SCREEN_H);
        lv_obj_set_pos(s_voice_overlay, 0, 0);
        lv_obj_add_flag(s_voice_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(s_voice_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(s_voice_overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_voice_overlay, LV_OPA_COVER, 0);
        ui_page_robot_face_build(s_voice_overlay, UI_ROBOT_FACE_LISTENING);
    } else {
        /* Repeat wakeups randomize the face without changing lock state. */
        ui_page_robot_face_set_mode(UI_ROBOT_FACE_LISTENING);
    }
    s_voice_until = lv_tick_get() + UI_VOICE_PREVIEW_MS;
    lv_obj_move_foreground(s_voice_overlay);
    /* Preserve both the lock and its indicator during voice interaction. */
    if (s_ui_locked && s_lock_overlay != NULL) lv_obj_move_foreground(s_lock_overlay);
}

static void voice_overlay_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (atomic_exchange_explicit(&s_voice_wake_pending, false,
                                  memory_order_acq_rel)) {
        voice_overlay_show();
    }
    if (s_voice_overlay != NULL &&
        (int32_t)(lv_tick_get() - s_voice_until) >= 0) {
        voice_overlay_close();
        if (!s_ui_locked) ui_mark_activity();
    }
}

void ui_mark_activity(void)
{
    s_last_activity_tick = lv_tick_get();
}

bool ui_navigation_transition_active(void)
{
    return s_transition_overlay != NULL ||
           s_transition_underlay != NULL ||
           s_transition_animating;
}

static void page_activity_cb(void *user_data)
{
    (void)user_data;
    ui_mark_activity();
}

static void feature_back_cb(void *user_data)
{
    (void)user_data;

    /*
     * HOME is deliberately kept alive underneath every feature page.
     * Returning must therefore only remove the feature overlay.  Rebuilding
     * HOME here would reset its horizontal scroll position and create a
     * visible jump after the finger-following back animation completes.
     */
    ui_page_feature_stop();
    s_top_page = UI_TOP_HOME;
    ui_mark_activity();
}

static void show_feature_page(ui_menu_action_t action)
{
    if (s_ui_locked || ui_navigation_transition_active() || ui_page_feature_active()) {
        return;
    }

    /*
     * Keep the existing HOME page alive underneath the function page.
     * The shared function shell is layered above it.  During a right-drag
     * return, only the function content moves, naturally revealing HOME.
     */
    s_top_page = UI_TOP_FEATURE;
    ui_mark_activity();

    ui_page_feature_build(lv_screen_active(),
                          action,
                          feature_back_cb,
                          NULL,
                          page_activity_cb,
                          NULL);

    /* Optional business hook: navigation no longer depends on this callback. */
    if (s_action_cb != NULL) {
        s_action_cb(action, s_action_user_data);
    }
}

static void home_menu_action_cb(ui_menu_action_t action, void *user_data)
{
    (void)user_data;
    show_feature_page(action);
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

static void animate_transition_to(int32_t end_y, bool commit);
static void settings_drag_event_cb(lv_event_t *e)
{
    if (s_top_page != UI_TOP_SETTINGS || s_transition_animating) return;
    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev) return;
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_settings_press);
        s_settings_tracking = true;
        ui_mark_activity();
        return;
    }
    if (!s_settings_tracking || (code != LV_EVENT_PRESSING &&
        code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST)) return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    int32_t dx = point.x - s_settings_press.x;
    int32_t dy = point.y - s_settings_press.y;
    ui_mark_activity();
    if (s_transition_target == UI_TRANSITION_NONE &&
        dy >= UI_TRANSITION_LOCK_DISTANCE && dy > iabs32(dx)) {
        s_transition_target = UI_TRANSITION_FROM_SETTINGS;
        s_transition_overlay = s_settings_root;
    }
    if (s_transition_target == UI_TRANSITION_FROM_SETTINGS &&
        s_transition_overlay == s_settings_root)
        lv_obj_set_y(s_settings_root, clamp_i32(dy, 0, UI_SCREEN_H));
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_settings_tracking = false;
        if (s_transition_target == UI_TRANSITION_FROM_SETTINGS &&
            s_transition_overlay == s_settings_root) {
            bool commit = code == LV_EVENT_RELEASED &&
                          dy >= UI_TRANSITION_COMMIT_DISTANCE;
            animate_transition_to(commit ? UI_SCREEN_H : 0, commit);
        }
    }
}

static void attach_settings_gestures(lv_obj_t *root)
{
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, settings_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(root, settings_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(root, settings_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(root, settings_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);
}

static void vertical_drag_cb(int32_t dx,
                             int32_t dy,
                             bool released,
                             bool cancelled,
                             void *user_data);

static void begin_transition(ui_transition_target_t target)
{
    if (s_transition_overlay != NULL ||
        s_transition_underlay != NULL ||
        s_transition_animating ||
        s_top_page == UI_TOP_FEATURE || s_top_page == UI_TOP_SETTINGS) {
        return;
    }

    s_transition_target = target;

    if (target == UI_TRANSITION_TO_SETTINGS) {
        s_settings_root = ui_page_settings_build(lv_screen_active(), page_activity_cb, NULL);
        if (!s_settings_root) { s_transition_target = UI_TRANSITION_NONE; return; }
        attach_settings_gestures(s_settings_root);
        s_transition_overlay = s_settings_root;
        lv_obj_set_y(s_settings_root, UI_SCREEN_H);
        lv_obj_move_foreground(s_settings_root);
        return;
    }
    if (target == UI_TRANSITION_TO_CLOCK) {
        /*
         * HOME -> CLOCK: the clock comes in from above and covers HOME.
         * This is the "cover" direction the user expects.
         */
        s_transition_overlay = create_transition_surface();
        lv_obj_set_pos(s_transition_overlay, 0, -UI_SCREEN_H);
        ui_page_clock_build(s_transition_overlay);
        lv_obj_move_foreground(s_transition_overlay);
        return;
    }

    /*
     * STANDBY -> HOME is the inverse visual operation:
     * HOME is already underneath, while the current standby page itself
     * follows the finger upward and reveals HOME below it.
     *
     * Do not slide HOME upward over the clock.  That was the previous bug.
     */
    s_transition_overlay = ui_page_standby_get_root();
    if (s_transition_overlay == NULL) {
        s_transition_target = UI_TRANSITION_NONE;
        return;
    }

    s_transition_underlay =
        ui_page_home_build(lv_screen_active(),
                           home_menu_action_cb,
                           NULL,
                           page_activity_cb,
                           NULL,
                           vertical_drag_cb,
                           NULL);

    lv_obj_move_background(s_transition_underlay);
    lv_obj_move_foreground(s_transition_overlay);
}

static void update_transition_position(int32_t dy)
{
    if (s_transition_overlay == NULL) {
        return;
    }

    if (s_transition_target == UI_TRANSITION_TO_SETTINGS) {
        int32_t progress = clamp_i32(-dy, 0, UI_SCREEN_H);
        lv_obj_set_y(s_transition_overlay, UI_SCREEN_H - progress);
    }
    else if (s_transition_target == UI_TRANSITION_TO_CLOCK) {
        int32_t progress = clamp_i32(dy, 0, UI_SCREEN_H);
        lv_obj_set_y(s_transition_overlay, -UI_SCREEN_H + progress);
    }
    else {
        /* STANDBY exits upward, revealing the stationary HOME underneath. */
        int32_t progress = clamp_i32(-dy, 0, UI_SCREEN_H);
        lv_obj_set_y(s_transition_overlay, -progress);
    }
}

static void finish_transition_commit(lv_anim_t *anim)
{
    (void)anim;

    s_transition_animating = false;
    s_transition_overlay = NULL;
    s_transition_underlay = NULL;

    if (s_transition_target == UI_TRANSITION_TO_SETTINGS) {
        s_transition_target = UI_TRANSITION_NONE;
        s_top_page = UI_TOP_SETTINGS;
        s_settings_tracking = false;
        ui_mark_activity();
    }
    else if (s_transition_target == UI_TRANSITION_FROM_SETTINGS) {
        s_transition_target = UI_TRANSITION_NONE;
        ui_page_settings_stop();
        if (s_settings_root) lv_obj_delete(s_settings_root);
        s_settings_root = NULL;
        s_settings_tracking = false;
        s_top_page = UI_TOP_HOME;
        ui_mark_activity();
    }
    else if (s_transition_target == UI_TRANSITION_TO_CLOCK) {
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

    if (s_transition_target == UI_TRANSITION_TO_SETTINGS) {
        ui_page_settings_stop();
        if (s_settings_root) lv_obj_delete(s_settings_root);
        s_settings_root = NULL;
    }
    else if (s_transition_target == UI_TRANSITION_FROM_SETTINGS) {
        if (s_settings_root) lv_obj_set_y(s_settings_root, 0);
    }
    else if (s_transition_target == UI_TRANSITION_TO_CLOCK) {
        /* The temporary clock overlay is discarded. */
        ui_page_clock_stop();

        if (s_transition_overlay != NULL) {
            lv_obj_delete(s_transition_overlay);
        }
    }
    else if (s_transition_target == UI_TRANSITION_TO_HOME) {
        /*
         * The moving object is the real standby page, so keep it.
         * Only discard the temporary HOME underlay and restore standby to y=0.
         */
        if (s_transition_overlay != NULL) {
            lv_obj_set_y(s_transition_overlay, 0);
        }

        if (s_transition_underlay != NULL) {
            lv_obj_delete(s_transition_underlay);
        }
    }

    s_transition_overlay = NULL;
    s_transition_underlay = NULL;
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
    if (s_ui_locked) return;
    ui_mark_activity();

    if (s_top_page == UI_TOP_FEATURE || s_top_page == UI_TOP_SETTINGS) {
        return;
    }

    int32_t ax = iabs32(dx);
    int32_t ay = iabs32(dy);

    if (s_transition_overlay == NULL && !s_transition_animating) {
        if (ay < UI_TRANSITION_LOCK_DISTANCE || ay <= ax) {
            return;
        }

        if (s_top_page == UI_TOP_HOME && dy < 0) {
            begin_transition(UI_TRANSITION_TO_SETTINGS);
        }
        else if (s_top_page == UI_TOP_HOME && dy > 0) {
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
        int32_t committed_y =
            s_transition_target == UI_TRANSITION_TO_CLOCK ||
            s_transition_target == UI_TRANSITION_TO_SETTINGS ? 0 : -UI_SCREEN_H;
        animate_transition_to(committed_y, true);
    }
    else {
        int32_t cancelled_y =
            s_transition_target == UI_TRANSITION_TO_CLOCK ? -UI_SCREEN_H :
            s_transition_target == UI_TRANSITION_TO_SETTINGS ? UI_SCREEN_H : 0;
        animate_transition_to(cancelled_y, false);
    }
}

static void standby_event_cb(ui_standby_event_t event, void *user_data)
{
    (void)user_data;
    if (s_ui_locked) return;
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

    if (ui_navigation_transition_active() || s_ui_locked || s_voice_overlay != NULL) {
        return;
    }

    uint32_t elapsed = lv_tick_get() - s_last_activity_tick;
    if (elapsed < UI_IDLE_TIMEOUT_MS) {
        return;
    }

    if (s_top_page != UI_TOP_STANDBY ||
        s_standby_view != UI_STANDBY_CLOCK) {
        ui_show_standby_clock();
    }
    s_ui_locked = true;
    refresh_lock_overlay();
}

/* A physical long hold is authoritative from any unlocked UI page.
 * Cancel interactive navigation before rebuilding the clock; otherwise an
 * animation could still refer to a page deleted by ui_show_standby_clock(). */
void ui_lock_from_back_hold(void)
{
    if (s_ui_locked) return;
    voice_overlay_close();
    if (s_transition_overlay != NULL) {
        lv_anim_delete(s_transition_overlay, NULL);
        finish_transition_cancel(NULL);
    }
    ui_show_standby_clock();
    s_ui_locked = true;
    refresh_lock_overlay();
}

void ui_handle_back_action(void)
{
    if (s_ui_locked) return;
    ui_mark_activity();
    /* Never delete a page that an in-flight LVGL animation still owns. */
    if (ui_navigation_transition_active()) return;

    if (s_top_page == UI_TOP_SETTINGS) {
        if (s_transition_target == UI_TRANSITION_NONE && !s_transition_animating) {
            s_transition_target = UI_TRANSITION_FROM_SETTINGS;
            s_transition_overlay = s_settings_root;
            animate_transition_to(UI_SCREEN_H, true);
        }
        return;
    }

    if (s_top_page == UI_TOP_FEATURE) {
        feature_back_cb(NULL);
        return;
    }

    if (s_top_page == UI_TOP_HOME) {
        ui_show_standby_clock();
    }
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
    if (s_ui_locked) return;
    voice_overlay_close();
    if (s_top_page == UI_TOP_SETTINGS) {
        ui_page_settings_stop();
        s_settings_root = NULL;
        s_settings_tracking = false;
    }
    ui_page_feature_stop();
    ui_page_standby_stop();
    s_top_page = UI_TOP_HOME;
    ui_mark_activity();

    ui_page_home_show(home_menu_action_cb,
                      NULL,
                      page_activity_cb,
                      NULL,
                      vertical_drag_cb,
                      NULL);
}

void ui_show_standby_clock(void)
{
    voice_overlay_close();
    if (s_top_page == UI_TOP_SETTINGS) {
        ui_page_settings_stop();
        s_settings_root = NULL;
        s_settings_tracking = false;
    }
    ui_page_feature_stop();
    s_top_page = UI_TOP_STANDBY;
    s_standby_view = UI_STANDBY_CLOCK;
    ui_mark_activity();

    ui_page_standby_show(UI_STANDBY_CLOCK,
                         true,
                         standby_event_cb,
                         NULL,
                         vertical_drag_cb,
                         NULL);
    if (s_ui_locked) refresh_lock_overlay();
}

void ui_init(void)
{
    ui_assets_init();

    /* Boot directly into the locked clock. Set the state before creating
     * standby so its lock overlay exists for the very first visible frame.
     * Voice wakeup is independent of this UI state. */
    s_ui_locked = true;

    if (s_idle_timer == NULL) {
        s_idle_timer = lv_timer_create(idle_timer_cb, 1000, NULL);
    }
    if (s_voice_timer == NULL) {
        s_voice_timer = lv_timer_create(voice_overlay_timer_cb, 100, NULL);
    }

    ui_show_standby_clock();
}
