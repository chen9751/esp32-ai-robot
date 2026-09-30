#include "ui_page_devices.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define UI_SCREEN_W 640
#define UI_SCREEN_H 172
#define UI_DEVICE_PAGE_COUNT 4
#define UI_AIRCON_VIEW_MAX 2

#define UI_COLOR_BG          lv_color_hex(0x000000)
#define UI_COLOR_FG          lv_color_hex(0xFFFFFF)
#define UI_COLOR_DIM         lv_color_hex(0x55555B)
#define UI_COLOR_DISABLED    lv_color_hex(0x303034)
#define UI_COLOR_FILL        lv_color_hex(0xFFFFFF)
#define UI_COLOR_FILL_TEXT   lv_color_hex(0x000000)

#define TEMP_MIN_X2 32
#define TEMP_MAX_X2 62
#define TEMP_DRAG_STEP_PX 12
#define FAN_MIN 1
#define FAN_MAX 7
#define FAN_TAP_SLOP 8

#if defined(UI_DEVICES_HAS_SOURCE_HAN)
LV_FONT_DECLARE(ui_font_source_han_devices_16);
LV_FONT_DECLARE(ui_font_source_han_devices_temp_44);
#define UI_DEVICES_FONT (&ui_font_source_han_devices_16)
#define UI_DEVICES_TEMP_FONT (&ui_font_source_han_devices_temp_44)
#else
#define UI_DEVICES_FONT LV_FONT_DEFAULT
#define UI_DEVICES_TEMP_FONT LV_FONT_DEFAULT
#endif

typedef struct {
    const char *title;
} ui_device_placeholder_t;

typedef struct {
    lv_obj_t *power_btn;
    lv_obj_t *mode_label;
    lv_obj_t *mode_dropdown;
    lv_obj_t *swing_btn;
    lv_obj_t *temp_label;
    lv_obj_t *temp_gesture;
    lv_obj_t *fan_label;
    lv_obj_t *fan_slider;
    lv_obj_t *feature_btn[4];
} ui_aircon_view_t;

enum {
    AIRCON_FEATURE_SLEEP = 0,
    AIRCON_FEATURE_ECO,
    AIRCON_FEATURE_DRY,
    AIRCON_FEATURE_AUX_HEAT,
};

static const ui_device_placeholder_t s_pages[] = {
    { "AIR CONDITIONER" },
    { "CURTAIN" },
    { "BATH HEATER" },
    { "DRYING RACK" },
};

static const char *s_feature_names[4] = {
    "睡眠", "ECO", "干燥", "辅热"
};

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_pager = NULL;
static bool s_wrapping = false;
static bool s_updating_views = false;
static ui_devices_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;

static ui_aircon_view_t s_aircon_views[UI_AIRCON_VIEW_MAX];
static size_t s_aircon_view_count = 0;

static bool s_power_on = true;
static uint8_t s_mode = 0;
static bool s_swing_on = false;
static bool s_features[4] = { false, false, false, false };
static int s_temp_x2 = 48;
static int s_fan_speed = 4;
static bool s_fan_auto = false;

static int32_t s_temp_last_y = 0;
static lv_point_t s_fan_press = {0, 0};
static int s_fan_at_press = 4;
static bool s_fan_dragged = false;

static void note_activity(void)
{
    if (s_activity_cb != NULL) s_activity_cb(s_activity_user_data);
}

static int32_t iabs32(int32_t v)
{
    return v < 0 ? -v : v;
}

static void set_enabled(lv_obj_t *obj, bool enabled)
{
    if (obj == NULL) return;
    if (enabled) lv_obj_remove_state(obj, LV_STATE_DISABLED);
    else lv_obj_add_state(obj, LV_STATE_DISABLED);
}

static void set_checked(lv_obj_t *obj, bool checked)
{
    if (obj == NULL) return;
    if (checked) lv_obj_add_state(obj, LV_STATE_CHECKED);
    else lv_obj_remove_state(obj, LV_STATE_CHECKED);
}

static void style_rounded_control(lv_obj_t *obj)
{
    lv_obj_set_style_radius(obj, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, UI_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, UI_COLOR_FG, LV_PART_MAIN);

    lv_obj_set_style_bg_color(obj, UI_COLOR_FILL, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(obj, UI_COLOR_FILL_TEXT, LV_PART_MAIN | LV_STATE_CHECKED);

    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_border_color(obj, UI_COLOR_DISABLED, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_text_color(obj, UI_COLOR_DIM, LV_PART_MAIN | LV_STATE_DISABLED);
}

static lv_obj_t *create_text_button(lv_obj_t *parent,
                                    const char *text,
                                    int32_t x,
                                    int32_t y,
                                    int32_t w,
                                    int32_t h,
                                    bool checkable)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    style_rounded_control(btn);
    if (checkable) lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_DEVICES_FONT, 0);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return btn;
}

static void update_aircon_views(void)
{
    char temp_text[16];
    char fan_text[16];

    s_updating_views = true;
    snprintf(temp_text, sizeof(temp_text), "%d.%d°", s_temp_x2 / 2, (s_temp_x2 & 1) ? 5 : 0);
    if (s_fan_auto) snprintf(fan_text, sizeof(fan_text), "AUTO");
    else snprintf(fan_text, sizeof(fan_text), "%d", s_fan_speed);

    for (size_t i = 0; i < s_aircon_view_count; ++i) {
        ui_aircon_view_t *v = &s_aircon_views[i];
        set_checked(v->power_btn, s_power_on);
        set_checked(v->swing_btn, s_swing_on);
        for (int j = 0; j < 4; ++j) set_checked(v->feature_btn[j], s_features[j]);

        set_enabled(v->mode_dropdown, s_power_on);
        set_enabled(v->swing_btn, s_power_on);
        set_enabled(v->temp_gesture, s_power_on);
        set_enabled(v->fan_slider, s_power_on);
        for (int j = 0; j < 4; ++j) set_enabled(v->feature_btn[j], s_power_on);

        lv_dropdown_set_selected(v->mode_dropdown, s_mode);
        lv_slider_set_value(v->fan_slider, s_fan_speed, LV_ANIM_OFF);

        if (s_power_on) {
            lv_label_set_text(v->temp_label, temp_text);
            lv_obj_set_style_text_font(v->temp_label, UI_DEVICES_TEMP_FONT, 0);
            lv_obj_set_style_text_color(v->temp_label, UI_COLOR_FG, 0);
            lv_label_set_text(v->fan_label, fan_text);
            lv_obj_set_style_text_color(v->fan_label, UI_COLOR_FG, 0);
            lv_obj_set_style_text_color(v->mode_label, UI_COLOR_FG, 0);
        } else {
            lv_label_set_text(v->temp_label, "温度计");
            lv_obj_set_style_text_font(v->temp_label, UI_DEVICES_FONT, 0);
            lv_obj_set_style_text_color(v->temp_label, UI_COLOR_DIM, 0);
            lv_label_set_text(v->fan_label, fan_text);
            lv_obj_set_style_text_color(v->fan_label, UI_COLOR_DIM, 0);
            lv_obj_set_style_text_color(v->mode_label, UI_COLOR_DIM, 0);
        }
    }
    s_updating_views = false;
}

static void power_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_power_on = !s_power_on;
    update_aircon_views();
    note_activity();
}

static void swing_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_power_on) return;
    s_swing_on = !s_swing_on;
    update_aircon_views();
    note_activity();
}

static void feature_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_power_on) return;
    intptr_t index = (intptr_t)lv_event_get_user_data(e);
    if (index < 0 || index >= 4) return;
    s_features[index] = !s_features[index];
    update_aircon_views();
    note_activity();
}

static void mode_event_cb(lv_event_t *e)
{
    lv_obj_t *dropdown = lv_event_get_target_obj(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *list = lv_dropdown_get_list(dropdown);
        if (list != NULL) lv_obj_set_style_text_font(list, UI_DEVICES_FONT, LV_PART_MAIN);
        note_activity();
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED && !s_updating_views && s_power_on) {
        s_mode = (uint8_t)lv_dropdown_get_selected(dropdown);
        update_aircon_views();
        note_activity();
    }
}

static void temp_event_cb(lv_event_t *e)
{
    if (!s_power_on) return;
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        s_temp_last_y = point.y;
        note_activity();
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        int32_t dy = point.y - s_temp_last_y;
        while (dy <= -TEMP_DRAG_STEP_PX && s_temp_x2 < TEMP_MAX_X2) {
            ++s_temp_x2;
            s_temp_last_y -= TEMP_DRAG_STEP_PX;
            dy += TEMP_DRAG_STEP_PX;
        }
        while (dy >= TEMP_DRAG_STEP_PX && s_temp_x2 > TEMP_MIN_X2) {
            --s_temp_x2;
            s_temp_last_y += TEMP_DRAG_STEP_PX;
            dy -= TEMP_DRAG_STEP_PX;
        }
        update_aircon_views();
        note_activity();
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) note_activity();
}

static void fan_event_cb(lv_event_t *e)
{
    if (!s_power_on || s_updating_views) return;
    lv_obj_t *slider = lv_event_get_target_obj(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED && indev != NULL) {
        lv_indev_get_point(indev, &s_fan_press);
        s_fan_at_press = s_fan_speed;
        s_fan_dragged = false;
        note_activity();
        return;
    }

    if (code == LV_EVENT_PRESSING && indev != NULL) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        if (iabs32(point.x - s_fan_press.x) > FAN_TAP_SLOP ||
            iabs32(point.y - s_fan_press.y) > FAN_TAP_SLOP) {
            s_fan_dragged = true;
        }
        if (s_fan_dragged) {
            s_fan_auto = false;
            s_fan_speed = lv_slider_get_value(slider);
            update_aircon_views();
        }
        note_activity();
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (s_fan_dragged) {
            s_fan_speed = lv_slider_get_value(slider);
            s_fan_auto = false;
            update_aircon_views();
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        if (!s_fan_dragged) {
            s_fan_speed = s_fan_at_press;
            s_fan_auto = !s_fan_auto;
        } else {
            s_fan_speed = lv_slider_get_value(slider);
            s_fan_auto = false;
        }
        update_aircon_views();
        note_activity();
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        update_aircon_views();
        note_activity();
    }
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

    if (code == LV_EVENT_SCROLL_END) normalize_loop_position();
}

static lv_obj_t *create_page(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(page, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    return page;
}

static void build_placeholder(lv_obj_t *parent, const char *title)
{
    lv_obj_t *page = create_page(parent);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(page);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, UI_COLOR_FG, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
}

static void build_aircon_page(lv_obj_t *parent)
{
    if (s_aircon_view_count >= UI_AIRCON_VIEW_MAX) return;

    ui_aircon_view_t *v = &s_aircon_views[s_aircon_view_count++];
    lv_obj_t *page = create_page(parent);

    v->power_btn = create_text_button(page, "开关", 64, 10, 120, 34, false);
    lv_obj_add_event_cb(v->power_btn, power_event_cb, LV_EVENT_CLICKED, NULL);

    v->mode_label = lv_label_create(page);
    lv_label_set_text(v->mode_label, "模式");
    lv_obj_set_style_text_font(v->mode_label, UI_DEVICES_FONT, 0);
    lv_obj_set_style_text_color(v->mode_label, UI_COLOR_FG, 0);
    lv_obj_set_pos(v->mode_label, 66, 49);
    lv_obj_clear_flag(v->mode_label, LV_OBJ_FLAG_CLICKABLE);

    v->mode_dropdown = lv_dropdown_create(page);
    lv_obj_set_pos(v->mode_dropdown, 64, 66);
    lv_obj_set_size(v->mode_dropdown, 120, 40);
    lv_dropdown_set_options(v->mode_dropdown, "制冷\n制热\n风扇\n干燥");
    lv_dropdown_set_selected(v->mode_dropdown, s_mode);
    style_rounded_control(v->mode_dropdown);
    lv_obj_set_style_text_font(v->mode_dropdown, UI_DEVICES_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_left(v->mode_dropdown, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(v->mode_dropdown, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(v->mode_dropdown, mode_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(v->mode_dropdown, mode_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    v->swing_btn = create_text_button(page, "摆风", 64, 126, 120, 34, true);
    lv_obj_add_event_cb(v->swing_btn, swing_event_cb, LV_EVENT_CLICKED, NULL);

    v->temp_label = lv_label_create(page);
    lv_obj_set_style_text_font(v->temp_label, UI_DEVICES_TEMP_FONT, 0);
    lv_obj_set_style_text_color(v->temp_label, UI_COLOR_FG, 0);
    lv_obj_set_width(v->temp_label, 238);
    lv_obj_set_style_text_align(v->temp_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(v->temp_label, 205, 24);
    lv_obj_clear_flag(v->temp_label, LV_OBJ_FLAG_CLICKABLE);

    v->temp_gesture = lv_obj_create(page);
    lv_obj_remove_style_all(v->temp_gesture);
    lv_obj_set_pos(v->temp_gesture, 205, 8);
    lv_obj_set_size(v->temp_gesture, 238, 96);
    lv_obj_set_style_bg_opa(v->temp_gesture, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(v->temp_gesture, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(v->temp_gesture, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(v->temp_gesture, temp_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(v->temp_gesture, temp_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(v->temp_gesture, temp_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(v->temp_gesture, temp_event_cb, LV_EVENT_PRESS_LOST, NULL);

    v->fan_label = lv_label_create(page);
    lv_obj_set_style_text_color(v->fan_label, UI_COLOR_FG, 0);
    lv_obj_set_pos(v->fan_label, 314, 109);
    lv_obj_set_width(v->fan_label, 40);
    lv_obj_set_style_text_align(v->fan_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(v->fan_label, LV_OBJ_FLAG_CLICKABLE);

    v->fan_slider = lv_slider_create(page);
    lv_obj_set_pos(v->fan_slider, 218, 139);
    lv_obj_set_size(v->fan_slider, 212, 12);
    lv_slider_set_range(v->fan_slider, FAN_MIN, FAN_MAX);
    lv_slider_set_value(v->fan_slider, s_fan_speed, LV_ANIM_OFF);
    lv_obj_set_style_radius(v->fan_slider, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(v->fan_slider, UI_COLOR_DIM, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(v->fan_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(v->fan_slider, UI_COLOR_FG, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(v->fan_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(v->fan_slider, UI_COLOR_FG, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(v->fan_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(v->fan_slider, 3, LV_PART_KNOB);
    lv_obj_set_style_bg_color(v->fan_slider, UI_COLOR_DISABLED, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(v->fan_slider, UI_COLOR_DIM, LV_PART_INDICATOR | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(v->fan_slider, UI_COLOR_DIM, LV_PART_KNOB | LV_STATE_DISABLED);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_PRESS_LOST, NULL);

    const int32_t right_x[2] = { 470, 552 };
    const int32_t right_y[2] = { 36, 96 };
    for (int i = 0; i < 4; ++i) {
        int col = i & 1;
        int row = i >> 1;
        v->feature_btn[i] = create_text_button(page,
                                               s_feature_names[i],
                                               right_x[col],
                                               right_y[row],
                                               72,
                                               42,
                                               true);
        lv_obj_add_event_cb(v->feature_btn[i],
                            feature_event_cb,
                            LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    update_aircon_views();
}

lv_obj_t *ui_page_devices_build(lv_obj_t *parent,
                                ui_devices_activity_cb_t activity_cb,
                                void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_wrapping = false;
    s_updating_views = false;
    s_aircon_view_count = 0;

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

    /* Clone the last page before page 0 for reverse looping. */
    build_placeholder(s_pager, s_pages[UI_DEVICE_PAGE_COUNT - 1].title);

    /* Real pages: Air Conditioner, Curtain, Bath Heater, Drying Rack. */
    build_aircon_page(s_pager);
    for (size_t i = 1; i < UI_DEVICE_PAGE_COUNT; ++i) {
        build_placeholder(s_pager, s_pages[i].title);
    }

    /* Clone page 0 after the last page for forward looping. */
    build_aircon_page(s_pager);

    lv_obj_add_event_cb(s_pager, pager_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_scroll_to_x(s_pager, UI_SCREEN_W, LV_ANIM_OFF);
    update_aircon_views();
    return s_root;
}

void ui_page_devices_stop(void)
{
    s_root = NULL;
    s_pager = NULL;
    s_wrapping = false;
    s_updating_views = false;
    s_aircon_view_count = 0;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}
