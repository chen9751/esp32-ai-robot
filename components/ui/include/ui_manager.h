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
 * - After 60 seconds without input, return to clock and lock the UI.
 * - Manually returning to clock locks after 60 seconds of inactivity.
 * - When unlocked, holding physical/custom back for 3 seconds locks
 *   immediately and returns to clock from any page.
 * - Voice wake does not alter the lock state.
 * - While locked, touch is blocked; hold the physical/custom back key
 *   for 3 seconds to unlock. Voice wakeup remains independent.
 * - Standby never remembers the previously open function page.
 * - A short tap on standby opens HOME.
 * - Upward standby drag and downward HOME drag are interactive/finger-following.
 */
void ui_show_main_menu(void);
void ui_show_standby_clock(void);

void ui_mark_activity(void);
bool ui_navigation_transition_active(void);

/* Physical/custom back key contract: alert acknowledgement first, then
 * feature -> HOME, HOME -> standby clock. */
void ui_handle_back_action(void);
bool ui_is_locked(void);
void ui_unlock_from_back_hold(void);
void ui_lock_from_back_hold(void);
/* Thread-safe event signal only; presentation runs on the LVGL timer. */
void ui_notify_voice_wakeup(void);

void ui_init(void);

#ifdef __cplusplus
}
#endif
