#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_music_activity_cb_t)(void *user_data);

typedef enum {
    UI_MUSIC_TARGET_TV = 0,
    UI_MUSIC_TARGET_SPEAKER,
} ui_music_target_t;

typedef enum {
    UI_MUSIC_ACTION_TARGET_CHANGED = 0,
    UI_MUSIC_ACTION_PREVIOUS,
    UI_MUSIC_ACTION_PLAY_PAUSE,
    UI_MUSIC_ACTION_NEXT,
    UI_MUSIC_ACTION_SEEK,
} ui_music_action_t;

typedef void (*ui_music_action_cb_t)(ui_music_action_t action,
                                     int32_t value,
                                     void *user_data);

void ui_page_music_build(lv_obj_t *parent,
                         ui_music_activity_cb_t activity_cb,
                         void *activity_user_data);
void ui_page_music_stop(void);

void ui_page_music_set_action_cb(ui_music_action_cb_t cb, void *user_data);
void ui_page_music_set_target(ui_music_target_t target);
ui_music_target_t ui_page_music_get_target(void);

/* UI state injection. Transport/network modules own the real playback state. */
void ui_page_music_set_metadata(const char *title,
                                const char *album,
                                const char *artist);
void ui_page_music_set_playback(bool playing,
                                int32_t position_seconds,
                                int32_t duration_seconds);

#ifdef __cplusplus
}
#endif
