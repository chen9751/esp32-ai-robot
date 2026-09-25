#include "ui_manager.h"
#include "ui_page_home.h"
#include "ui_page_standby.h"
#include "ui_assets.h"

#include <stdint.h>

#define UI_IDLE_TIMEOUT_MS 60000u

typedef enum {
    UI_TOP_STANDBY = 0,
    UI_TOP_HOME,
} ui_top_page_t;

static ui_menu_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static const lv_font_t *s_menu_font = NULL;

static ui_top_page_t s_top_page = UI_TOP_STANDBY;
static ui_standby_view_t s_standby_view = UI_STANDBY_CLOCK;
static uint32_t s_last_activity_tick = 0;
static lv_timer_t *s_idle_timer = NULL;

void ui_mark_activity(void)
{
    s_last_activity_tick = lv_tick_get();
}

static void page_activity_cb(void *user_data)
{
    (void)user_data;
    ui_mark_activity();
}

static void global_input_event_cb(lv_event_t *e)
{
    (void)e;
    ui_mark_activity();
}

static void bind_global_input_activity(void)
{
    lv_indev_t *indev = NULL;

    while ((indev = lv_indev_get_next(indev)) != NULL) {
        lv_indev_add_event_cb(indev,
                              global_input_event_cb,
                              LV_EVENT_PRESSED,
                              NULL);
        lv_indev_add_event_cb(indev,
                              global_input_event_cb,
                              LV_EVENT_KEY,
                              NULL);
    }
}

static void standby_event_cb(ui_standby_event_t event, void *user_data)
{
    (void)user_data;
    ui_mark_activity();

    if (event == UI_STANDBY_EVENT_OPEN_HOME) {
        ui_show_main_menu();
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
                         NULL);
}

static void idle_timer_cb(lv_timer_t *timer)
{
    (void)timer;

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
    /*
     * Reserved for future dynamic CJK pages. The current HOME labels are
     * compact A8 assets and the standby clock uses its own pixel renderer.
     */
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
                         NULL);
}

void ui_init(void)
{
    ui_assets_init();
    bind_global_input_activity();

    if (s_idle_timer == NULL) {
        s_idle_timer = lv_timer_create(idle_timer_cb, 1000, NULL);
    }

    ui_show_standby_clock();
}
