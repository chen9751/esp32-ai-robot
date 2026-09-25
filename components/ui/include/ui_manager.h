#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_MENU_REMOTE = 0,
    UI_MENU_MUSIC,
    UI_MENU_LIGHTS,
    UI_MENU_DEVICES,
    UI_MENU_ALARM,
    UI_MENU_SETTINGS,
} ui_menu_action_t;

typedef void (*ui_menu_action_cb_t)(ui_menu_action_t action, void *user_data);

/* Optional: set a CJK-capable LVGL font before ui_init(). */
void ui_set_menu_font(const lv_font_t *font);

/* Register the page-router callback for the six touch menu entries. */
void ui_set_menu_action_cb(ui_menu_action_cb_t cb, void *user_data);

/*
 * Navigation contract:
 * - Any page returns to the standby clock after 60 seconds without input.
 * - The standby clock never remembers the previously open page.
 * - A short tap or upward swipe on any standby subpage always opens HOME.
 */
void ui_show_main_menu(void);
void ui_show_standby_clock(void);

/* Can be called by future pages after programmatic/user activity. */
void ui_mark_activity(void);

void ui_init(void);

#ifdef __cplusplus
}
#endif
