#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_settings_activity_cb_t)(void *user_data);

typedef struct {
    void (*volume_changed)(int32_t percent, void *user_data);
    void (*brightness_changed)(int32_t percent, void *user_data);
    void (*wifi_enabled_changed)(bool enabled, void *user_data);
    void (*bluetooth_enabled_changed)(bool enabled, void *user_data);
} ui_settings_callbacks_t;

/*
 * Settings is a presentation layer only. Application/board/network code owns
 * the real hardware and service state and connects it through this bridge.
 * The callbacks are invoked from LVGL event context and must stay non-blocking.
 */
void ui_page_settings_set_callbacks(const ui_settings_callbacks_t *callbacks,
                                    void *user_data);

/*
 * State injection for board/network/AI services. If a setter is called from a
 * task other than the LVGL task, the caller must hold the platform LVGL lock.
 * These functions may be called before the settings page exists; the value is
 * retained and shown the next time the page is opened.
 */
void ui_page_settings_set_volume(int32_t percent);
void ui_page_settings_set_brightness(int32_t percent);
void ui_page_settings_set_wifi_state(bool enabled,
                                     bool configured,
                                     bool connected);
void ui_page_settings_set_bluetooth_enabled(bool enabled);
void ui_page_settings_set_ai_state(bool configured, bool online);

lv_obj_t *ui_page_settings_build(lv_obj_t *parent,
                                 ui_settings_activity_cb_t activity_cb,
                                 void *activity_user_data);

void ui_page_settings_stop(void);

#ifdef __cplusplus
}
#endif
