#pragma once

#include "ui_manager.h"

typedef void (*ui_page_activity_cb_t)(void *user_data);

lv_obj_t *ui_page_home_build(lv_obj_t *parent,
                             ui_menu_action_cb_t action_cb,
                             void *action_user_data,
                             ui_page_activity_cb_t activity_cb,
                             void *activity_user_data);

void ui_page_home_show(ui_menu_action_cb_t action_cb,
                       void *action_user_data,
                       ui_page_activity_cb_t activity_cb,
                       void *activity_user_data);
