#pragma once

#include "lvgl.h"

/* LVGL 9.2.2 provides discrete named opacity constants and does not define
 * LV_OPA_55. Keep the devices page compatible with the project's baseline by
 * mapping that visual track opacity to the nearest supported constant. */
#ifndef LV_OPA_55
#define LV_OPA_55 LV_OPA_50
#endif

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
 */
lv_obj_t *ui_page_devices_build(lv_obj_t *parent,
                                ui_devices_activity_cb_t activity_cb,
                                void *activity_user_data);

/* Update the Curtain page from external device/HA state.
 * `percent` is opening percentage: 0 = fully closed, 100 = fully open. */
void ui_page_devices_set_curtain_position(uint8_t percent);

/* Update the Bath Heater's detected room temperature from external state.
 * `temperature_x10` uses tenths of a degree, e.g. 243 = 24.3. */
void ui_page_devices_set_bath_current_temperature(int16_t temperature_x10);

/* Update the Drying Rack page from external device/HA state.
 * `percent` is vertical travel: 0 = top, 100 = lowest visual position. */
void ui_page_devices_set_drying_rack_position(uint8_t percent);

void ui_page_devices_stop(void);

#ifdef __cplusplus
}
#endif
