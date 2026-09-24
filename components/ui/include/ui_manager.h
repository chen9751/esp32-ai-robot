#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {UI_MUSIC_PREV,UI_MUSIC_PLAY_PAUSE,UI_MUSIC_NEXT} ui_music_action_t;
typedef enum {UI_REMOTE_UP,UI_REMOTE_DOWN,UI_REMOTE_LEFT,UI_REMOTE_RIGHT,UI_REMOTE_OK,UI_REMOTE_BACK,UI_REMOTE_HOME,UI_REMOTE_PLAY_PAUSE,UI_REMOTE_VOL_UP,UI_REMOTE_VOL_DOWN,UI_REMOTE_POWER,UI_REMOTE_INPUT,UI_REMOTE_SETTINGS,UI_REMOTE_DISPLAY} ui_remote_action_t;
void ui_init(void);
void ui_show_home(void);
void ui_show_functions(void);
void ui_show_settings(void);

/* Override these weak hooks in service/board modules. UI controls are already wired to them. */
void ui_action_volume(int value);
void ui_action_brightness(int value);
void ui_action_light_select(int index);
void ui_action_light_power(bool on);
void ui_action_light_brightness(int value);
void ui_action_light_temperature(int value);
void ui_action_light_preset(int preset);
void ui_action_music(ui_music_action_t action);
void ui_action_music_seek(int percent);
void ui_action_remote(ui_remote_action_t action);
void ui_action_orientation(bool portrait);
#ifdef __cplusplus
}
#endif
