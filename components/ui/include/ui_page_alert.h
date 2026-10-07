#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_alert_dismiss_cb_t)(void *user_data);

/* Full-screen ringing overlay used by alarm alerts. The previous page remains
 * alive underneath so dismissing the overlay returns exactly where the user
 * was before the alarm fired. */
lv_obj_t *ui_page_alert_build(lv_obj_t *parent,
                              ui_alert_dismiss_cb_t dismiss_cb,
                              void *user_data);
void ui_page_alert_stop(void);
bool ui_page_alert_active(void);

#ifdef __cplusplus
}
#endif
