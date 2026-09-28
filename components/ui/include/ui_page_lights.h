#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_lights_activity_cb_t)(void *user_data);

lv_obj_t *ui_page_lights_build(lv_obj_t *parent,
                               ui_lights_activity_cb_t activity_cb,
                               void *activity_user_data);
void ui_page_lights_stop(void);

#ifdef __cplusplus
}
#endif
