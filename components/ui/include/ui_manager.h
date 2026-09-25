#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

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

/*
 * Continuous drag callback used by pages that participate in the vertical
 * HOME <-> standby interactive transition.
 */
typedef void (*ui_vertical_drag_cb_t)(int32_t dx,
                                      int32_t dy,
                                      bool released,
                                      bool cancelled,
                                      void *user_data);

/* Reserved for future dynamic CJK pages; current HOME labels are A8 assets. */
void ui_set_menu_font(const lv_font_t *font);
void ui_set_menu_action_cb(ui_menu_action_cb_t cb, void *user_data);

/*
 * Navigation contract:
 * - Any page returns to the standby clock after 60 seconds without input.
 * - Standby never remembers the previously open function page.
 * - A short tap on standby opens HOME.
 * - Upward standby drag and downward HOME drag are interactive/finger-following.
 */
void ui_show_main_menu(void);
void ui_show_standby_clock(void);

void ui_mark_activity(void);
bool ui_navigation_transition_active(void);

void ui_init(void);

#ifdef __cplusplus
}
#endif
