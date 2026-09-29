#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_remote_activity_cb_t)(void *user_data);

typedef enum {
    UI_REMOTE_ACTION_POWER = 0,
    UI_REMOTE_ACTION_INPUT,
    UI_REMOTE_ACTION_HOME,
    UI_REMOTE_ACTION_BACK,
    UI_REMOTE_ACTION_SETTINGS,
    UI_REMOTE_ACTION_DISPLAY,
    UI_REMOTE_ACTION_VOLUME_UP,
    UI_REMOTE_ACTION_VOLUME_DOWN,
    UI_REMOTE_ACTION_OK,
    UI_REMOTE_ACTION_SWIPE_LEFT,
    UI_REMOTE_ACTION_SWIPE_RIGHT,
    UI_REMOTE_ACTION_SWIPE_UP,
    UI_REMOTE_ACTION_SWIPE_DOWN,
} ui_remote_action_t;

typedef void (*ui_remote_action_cb_t)(ui_remote_action_t action,
                                      int32_t value,
                                      void *user_data);

void ui_page_remote_build(lv_obj_t *parent,
                          ui_remote_activity_cb_t activity_cb,
                          void *activity_user_data);
void ui_page_remote_stop(void);
void ui_page_remote_set_action_cb(ui_remote_action_cb_t cb, void *user_data);

/* Rotate only the eight icon image objects. Button geometry, touchpad geometry,
 * hit areas and the 640x172 layout remain unchanged. */
void ui_page_remote_set_icons_ccw90(bool ccw90, bool animate);

#ifdef __cplusplus
}
#endif
