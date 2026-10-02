#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_devices_activity_cb_t)(void *user_data);

/*
 * Four full-screen device pages, horizontally paged in this order:
 * Air Conditioner -> Curtain -> Bath Heater -> Drying Rack.
 *
 * This module owns presentation and paging only. Device/HA transport logic
 * must remain outside the UI layer.
 *
 * Threading contract: all functions in this module that can touch live LVGL
 * objects must be called from the LVGL/UI thread (or while holding the same
 * LVGL lock used by the project). Network/HA worker tasks should marshal state
 * updates to the UI thread instead of calling these setters directly.
 */
lv_obj_t *ui_page_devices_build(lv_obj_t *parent,
                                ui_devices_activity_cb_t activity_cb,
                                void *activity_user_data);

/* Update the Curtain page from external device/HA state.
 * `percent` is opening percentage: 0 = fully closed, 100 = fully open.
 * Call from the LVGL/UI thread; see the threading contract above. */
void ui_page_devices_set_curtain_position(uint8_t percent);

/* Update the Bath Heater's detected room temperature from external state.
 * `temperature_x10` uses tenths of a degree, e.g. 243 = 24.3.
 * Call from the LVGL/UI thread; see the threading contract above. */
void ui_page_devices_set_bath_current_temperature(int16_t temperature_x10);

/* Update the Drying Rack page from external device/HA state.
 * `percent` is vertical travel: 0 = top, 100 = lowest visual position.
 * Call from the LVGL/UI thread; see the threading contract above. */
void ui_page_devices_set_drying_rack_position(uint8_t percent);

void ui_page_devices_stop(void);

#ifdef __cplusplus
}
#endif
