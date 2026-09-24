#include "ui_manager.h"
#include "lvgl.h"

void ui_init(void)
{
    /*
     * Clean LVGL baseline.
     *
     * Do not create project-specific pages, widgets, gestures, styles,
     * mock data, service hooks, or orientation logic here yet.
     * LVGL's active screen is left in its default initial state so the
     * next UI design can be built from scratch.
     */
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
}
