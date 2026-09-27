#pragma once

#include "ui_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_feature_back_cb_t)(void *user_data);
typedef void (*ui_feature_activity_cb_t)(void *user_data);

/*
 * Build the shared function-page shell above the current screen contents.
 *
 * The shell intentionally occupies the full 640x172 canvas.  Its left 56 px
 * are not a visual sidebar: they are only an invisible interaction rail with
 * a fixed vertical indicator drawn above the moving page content.
 *
 * Back interaction:
 * - tap the rail: go back
 * - press and drag right: content follows the finger
 * - release after the commit threshold: go back
 * - otherwise: content springs back to x=0
 */
lv_obj_t *ui_page_feature_build(lv_obj_t *parent,
                                ui_menu_action_t action,
                                ui_feature_back_cb_t back_cb,
                                void *back_user_data,
                                ui_feature_activity_cb_t activity_cb,
                                void *activity_user_data);

void ui_page_feature_stop(void);
bool ui_page_feature_active(void);

#ifdef __cplusplus
}
#endif
