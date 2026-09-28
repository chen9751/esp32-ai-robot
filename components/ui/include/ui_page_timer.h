#pragma once

#include "lvgl.h"

typedef void (*ui_timer_activity_cb_t)(void *user_data);

void ui_page_timer_build(lv_obj_t *parent,
                         ui_timer_activity_cb_t activity_cb,
                         void *activity_user_data);

/* Detaches page widgets only. An active countdown intentionally keeps running. */
void ui_page_timer_stop(void);
