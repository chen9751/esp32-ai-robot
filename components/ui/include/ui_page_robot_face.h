#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Standalone 640x172 full-screen robot face. No navigation, audio or lock
 * state changes occur here. The caller owns the parent and its lifetime. */
typedef enum {
    UI_ROBOT_FACE_SMILE = 0,
    UI_ROBOT_FACE_LISTENING,
} ui_robot_face_mode_t;

lv_obj_t *ui_page_robot_face_build(lv_obj_t *parent, ui_robot_face_mode_t mode);
void ui_page_robot_face_set_mode(ui_robot_face_mode_t mode);
void ui_page_robot_face_stop(void);

#ifdef __cplusplus
}
#endif
