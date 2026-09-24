#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the LVGL UI layer.
 * The project intentionally starts from a clean LVGL screen.
 * New UI pages and interaction logic will be designed from scratch.
 */
void ui_init(void);

#ifdef __cplusplus
}
#endif
