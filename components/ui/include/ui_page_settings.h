#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_settings_activity_cb_t)(void *user_data);

lv_obj_t *ui_page_settings_build(lv_obj_t *parent,
                                 ui_settings_activity_cb_t activity_cb,
                                 void *activity_user_data);

/* Connected = bright Wi-Fi icon; disconnected = muted gray icon. */
void ui_page_settings_set_wifi_connected(bool connected);

void ui_page_settings_stop(void);

#ifdef __cplusplus
}
#endif
