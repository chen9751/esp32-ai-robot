#include "ui_page_devices.h"

#include <stddef.h>

#define UI_SCREEN_W 640
#define UI_SCREEN_H 172
#define UI_DEVICE_PAGE_COUNT 4

#define UI_COLOR_BG lv_color_hex(0x000000)
#define UI_COLOR_FG lv_color_hex(0xFFFFFF)

typedef struct {
    const char *title;
} ui_device_placeholder_t;

static const ui_device_placeholder_t s_pages[] = {
    { "AIR CONDITIONER" },
    { "CURTAIN" },
    { "BATH HEATER" },
    { "DRYING RACK" },
};

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_pager = NULL;
static bool s_wrapping = false;
static ui_devices_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;

static void note_activity(void)
{
    if (s_activity_cb != NULL) s_activity_cb(s_activity_user_data);
}

static void normalize_loop_position(void)
{
    if (s_pager == NULL || s_wrapping) return;

    int32_t x = lv_obj_get_scroll_x(s_pager);
    int32_t first_clone_x = 0;
    int32_t last_clone_x = (UI_DEVICE_PAGE_COUNT + 1) * UI_SCREEN_W;

    s_wrapping = true;
    if (x <= first_clone_x) {
        lv_obj_scroll_to_x(s_pager, UI_DEVICE_PAGE_COUNT * UI_SCREEN_W, LV_ANIM_OFF);
    } else if (x >= last_clone_x) {
        lv_obj_scroll_to_x(s_pager, UI_SCREEN_W, LV_ANIM_OFF);
    }
    s_wrapping = false;
}

static void pager_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED ||
        code == LV_EVENT_PRESSING ||
        code == LV_EVENT_SCROLL_BEGIN ||
        code == LV_EVENT_SCROLL ||
        code == LV_EVENT_SCROLL_END ||
        code == LV_EVENT_RELEASED) {
        note_activity();
    }

    if (code == LV_EVENT_SCROLL_END) {
        normalize_loop_position();
    }
}

static void build_placeholder(lv_obj_t *parent, const char *title)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(page, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(page);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, UI_COLOR_FG, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t *ui_page_devices_build(lv_obj_t *parent,
                                ui_devices_activity_cb_t activity_cb,
                                void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_wrapping = false;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    s_pager = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_pager);
    lv_obj_set_size(s_pager, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_pager, 0, 0);
    lv_obj_set_style_bg_color(s_pager, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_pager, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_pager, 0, 0);
    lv_obj_set_style_pad_column(s_pager, 0, 0);
    lv_obj_set_scroll_dir(s_pager, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(s_pager, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_flag(s_pager, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_flex_flow(s_pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_pager,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    /* Edge clones make the pager feel continuous in both directions. */
    build_placeholder(s_pager, s_pages[UI_DEVICE_PAGE_COUNT - 1].title);
    for (size_t i = 0; i < UI_DEVICE_PAGE_COUNT; ++i) {
        build_placeholder(s_pager, s_pages[i].title);
    }
    build_placeholder(s_pager, s_pages[0].title);

    lv_obj_add_event_cb(s_pager, pager_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_scroll_to_x(s_pager, UI_SCREEN_W, LV_ANIM_OFF);
    return s_root;
}

void ui_page_devices_stop(void)
{
    s_root = NULL;
    s_pager = NULL;
    s_wrapping = false;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}
