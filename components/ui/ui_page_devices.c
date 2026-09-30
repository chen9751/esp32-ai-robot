#include "ui_page_devices.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define UI_SCREEN_W 640
#define UI_SCREEN_H 172
#define UI_DEVICE_PAGE_COUNT 4
#define UI_AIRCON_VIEW_MAX 2

#define COLOR_BG          lv_color_hex(0x000000)
#define COLOR_BUTTON      lv_color_hex(0x111824)
#define COLOR_BUTTON_2    lv_color_hex(0x171F2D)
#define COLOR_BORDER      lv_color_hex(0x526581)
#define COLOR_BORDER_ON   lv_color_hex(0x7897C8)
#define COLOR_PRESSED     lv_color_hex(0x24344D)
#define COLOR_CHECKED     lv_color_hex(0x22324A)
#define COLOR_FG          lv_color_hex(0xEAF2FF)
#define COLOR_DIM         lv_color_hex(0x536071)
#define COLOR_DISABLED_BG lv_color_hex(0x090D14)
#define COLOR_DISABLED_BR lv_color_hex(0x263142)
#define COLOR_POWER       lv_color_hex(0xFF604F)

#define TEMP_MIN_X2 32
#define TEMP_MAX_X2 62
#define TEMP_DRAG_STEP_PX 12
#define FAN_MIN 1
#define FAN_MAX 7
#define FAN_TAP_SLOP 8

/* Remix Icon font glyphs (v4.x). */
#define RI_POWER      "\xEF\x84\xA6" /* shut-down-line U+F126 */
#define RI_COOL       "\xEF\x94\x92" /* snowflake-line U+F512 */
#define RI_HEAT       "\xEF\x86\xBF" /* sun-line U+F1BF */
#define RI_FAN        "\xEF\x8B\x8A" /* windy-line U+F2CA */
#define RI_DRY        "\xEE\xB1\xAA" /* drop-line U+EC6A */
#define RI_SWING      "\xEE\xA9\xA2" /* arrow-left-right-line U+EA62 */
#define RI_THERMOMETER "\xEF\x87\xB2" /* temp-cold-line U+F1F2 */
#define MODE_OPTIONS RI_COOL "\n" RI_HEAT "\n" RI_FAN "\n" RI_DRY

#if defined(UI_DEVICES_HAS_FONTS)
LV_FONT_DECLARE(ui_font_source_han_devices_16);
LV_FONT_DECLARE(ui_font_source_han_devices_temp_72);
LV_FONT_DECLARE(ui_font_remix_devices_28);
LV_FONT_DECLARE(ui_font_remix_devices_56);
#define UI_TEXT_FONT       (&ui_font_source_han_devices_16)
#define UI_TEMP_FONT       (&ui_font_source_han_devices_temp_72)
#define UI_ICON_FONT       (&ui_font_remix_devices_28)
#define UI_ICON_LARGE_FONT (&ui_font_remix_devices_56)
#else
#define UI_TEXT_FONT       LV_FONT_DEFAULT
#define UI_TEMP_FONT       LV_FONT_DEFAULT
#define UI_ICON_FONT       LV_FONT_DEFAULT
#define UI_ICON_LARGE_FONT LV_FONT_DEFAULT
#endif

typedef struct {
    const char *title;
} ui_device_placeholder_t;

typedef struct {
    lv_obj_t *power_btn;
    lv_obj_t *power_icon;
    lv_obj_t *mode_dropdown;
    lv_obj_t *swing_btn;
    lv_obj_t *swing_icon;
    lv_obj_t *temp_label;
    lv_obj_t *temp_gesture;
    lv_obj_t *fan_value_label;
    lv_obj_t *fan_slider;
    lv_obj_t *fan_auto_label;
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
static bool s_fan_value_visible = false;

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

static void set_hidden(lv_obj_t *obj, bool hidden)
{
    if (obj == NULL) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void style_remote_control(lv_obj_t *obj, int32_t radius)
{
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, COLOR_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(obj, COLOR_BUTTON_2, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_opa(obj, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, COLOR_FG, LV_PART_MAIN);

    lv_obj_set_style_bg_color(obj, COLOR_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(obj, COLOR_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(obj, COLOR_BORDER_ON, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_bg_color(obj, COLOR_CHECKED, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_color(obj, COLOR_PRESSED, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(obj, COLOR_BORDER_ON, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);

    lv_obj_set_style_bg_color(obj, COLOR_DISABLED_BG, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_grad_color(obj, COLOR_DISABLED_BG, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_border_color(obj, COLOR_DISABLED_BR, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_border_opa(obj, LV_OPA_60, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_text_color(obj, COLOR_DIM, LV_PART_MAIN | LV_STATE_DISABLED);
}

static lv_obj_t *create_icon_button(lv_obj_t *parent,
                                    const char *glyph,
                                    int32_t x,
                                    int32_t y,
                                    int32_t w,
                                    int32_t h,
                                    bool checkable,
                                    lv_obj_t **icon_out)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    style_remote_control(btn, 21);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    if (checkable) lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, glyph);
    lv_obj_set_style_text_font(icon, UI_ICON_FONT, 0);
    lv_obj_center(icon);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    if (icon_out != NULL) *icon_out = icon;
    return btn;
}

static lv_obj_t *create_text_button(lv_obj_t *parent,
                                    const char *text,
                                    int32_t x,
                                    int32_t y,
                                    int32_t w,
                                    int32_t h)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    style_remote_control(btn, 22);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_TEXT_FONT, 0);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return btn;
}

static void style_mode_dropdown(lv_obj_t *dropdown)
{
    style_remote_control(dropdown, 21);
    lv_obj_set_style_text_font(dropdown, UI_ICON_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_align(dropdown, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dropdown, 0, LV_PART_MAIN);
    lv_dropdown_set_symbol(dropdown, NULL);
}

static void style_mode_list(lv_obj_t *list)
{
    if (list == NULL) return;
    lv_obj_set_style_bg_color(list, COLOR_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_radius(list, 18, LV_PART_MAIN);
    lv_obj_set_style_text_font(list, UI_ICON_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_color(list, COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_align(list, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(list, COLOR_CHECKED, LV_PART_SELECTED);
    lv_obj_set_style_text_color(list, COLOR_FG, LV_PART_SELECTED);
    lv_obj_set_style_pad_ver(list, 8, LV_PART_MAIN);
}

static void update_aircon_views(void)
{
    char temp_text[16];
    char fan_text[8];

    s_updating_views = true;
    snprintf(temp_text, sizeof(temp_text), "%d.%d°", s_temp_x2 / 2, (s_temp_x2 & 1) ? 5 : 0);
    snprintf(fan_text, sizeof(fan_text), "%d", s_fan_speed);

    for (size_t i = 0; i < s_aircon_view_count; ++i) {
        ui_aircon_view_t *v = &s_aircon_views[i];

        set_checked(v->power_btn, s_power_on);
        set_checked(v->swing_btn, s_swing_on);
        for (int j = 0; j < 4; ++j) set_checked(v->feature_btn[j], s_features[j]);

        set_enabled(v->mode_dropdown, s_power_on);
        set_enabled(v->swing_btn, s_power_on);
        set_enabled(v->temp_gesture, s_power_on);
        set_enabled(v->fan_slider, s_power_on);
        set_enabled(v->fan_auto_label, s_power_on);
        for (int j = 0; j < 4; ++j) set_enabled(v->feature_btn[j], s_power_on);

        lv_dropdown_set_selected(v->mode_dropdown, s_mode);
        lv_slider_set_value(v->fan_slider, s_fan_speed, LV_ANIM_OFF);

        /* Power remains the only bright control when the air conditioner is off. */
        lv_obj_set_style_text_color(v->power_icon, COLOR_POWER, 0);

        if (s_power_on) {
            lv_label_set_text(v->temp_label, temp_text);
            lv_obj_set_style_text_font(v->temp_label, UI_TEMP_FONT, 0);
            lv_obj_set_style_text_color(v->temp_label, COLOR_FG, 0);
            lv_obj_set_style_text_color(v->swing_icon, COLOR_FG, 0);
        } else {
            lv_label_set_text(v->temp_label, RI_THERMOMETER);
            lv_obj_set_style_text_font(v->temp_label, UI_ICON_LARGE_FONT, 0);
            lv_obj_set_style_text_color(v->temp_label, COLOR_DIM, 0);
            lv_obj_set_style_text_color(v->swing_icon, COLOR_DIM, 0);
        }

        if (s_fan_auto) {
            set_hidden(v->fan_slider, true);
            set_hidden(v->fan_value_label, true);
            set_hidden(v->fan_auto_label, false);
            lv_obj_set_style_text_color(v->fan_auto_label, s_power_on ? COLOR_FG : COLOR_DIM, 0);
        } else {
            set_hidden(v->fan_slider, false);
            set_hidden(v->fan_auto_label, true);
            set_hidden(v->fan_value_label, !s_fan_value_visible);
            lv_label_set_text(v->fan_value_label, fan_text);
            lv_obj_set_style_text_color(v->fan_value_label, s_power_on ? COLOR_FG : COLOR_DIM, 0);
        }
    }
    s_updating_views = false;
}

static void power_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_power_on = !s_power_on;
    s_fan_value_visible = false;
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
        style_mode_list(lv_dropdown_get_list(dropdown));
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
        bool changed = false;
        while (dy <= -TEMP_DRAG_STEP_PX && s_temp_x2 < TEMP_MAX_X2) {
            ++s_temp_x2;
            s_temp_last_y -= TEMP_DRAG_STEP_PX;
            dy += TEMP_DRAG_STEP_PX;
            changed = true;
        }
        while (dy >= TEMP_DRAG_STEP_PX && s_temp_x2 > TEMP_MIN_X2) {
            --s_temp_x2;
            s_temp_last_y += TEMP_DRAG_STEP_PX;
            dy -= TEMP_DRAG_STEP_PX;
            changed = true;
        }
        if (changed) update_aircon_views();
        note_activity();
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) note_activity();
}

static void fan_auto_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_power_on || !s_fan_auto) return;
    s_fan_auto = false;
    s_fan_value_visible = false;
    update_aircon_views();
    note_activity();
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
        s_fan_value_visible = false;
        note_activity();
        return;
    }

    if (code == LV_EVENT_PRESSING && indev != NULL) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        if (iabs32(point.x - s_fan_press.x) > FAN_TAP_SLOP ||
            iabs32(point.y - s_fan_press.y) > FAN_TAP_SLOP) {
            s_fan_dragged = true;
            s_fan_value_visible = true;
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
            s_fan_value_visible = true;
            update_aircon_views();
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        if (!s_fan_dragged) {
            s_fan_speed = s_fan_at_press;
            s_fan_auto = true;
        } else {
            s_fan_speed = lv_slider_get_value(slider);
            s_fan_auto = false;
        }
        s_fan_value_visible = false;
        update_aircon_views();
        note_activity();
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        s_fan_value_visible = false;
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
    lv_obj_set_style_bg_color(page, COLOR_BG, 0);
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
    lv_obj_set_style_text_color(label, COLOR_FG, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
}

static void build_aircon_page(lv_obj_t *parent)
{
    if (s_aircon_view_count >= UI_AIRCON_VIEW_MAX) return;

    ui_aircon_view_t *v = &s_aircon_views[s_aircon_view_count++];
    lv_obj_t *page = create_page(parent);

    /* Left column: icon-only controls, clear of the shared 56 px back rail. */
    v->power_btn = create_icon_button(page, RI_POWER, 66, 13, 112, 42, false, &v->power_icon);
    lv_obj_set_style_text_color(v->power_icon, COLOR_POWER, 0);
    lv_obj_add_event_cb(v->power_btn, power_event_cb, LV_EVENT_CLICKED, NULL);

    v->mode_dropdown = lv_dropdown_create(page);
    lv_obj_set_pos(v->mode_dropdown, 66, 65);
    lv_obj_set_size(v->mode_dropdown, 112, 42);
    lv_dropdown_set_options_static(v->mode_dropdown, MODE_OPTIONS);
    lv_dropdown_set_selected(v->mode_dropdown, s_mode);
    style_mode_dropdown(v->mode_dropdown);
    lv_obj_add_event_cb(v->mode_dropdown, mode_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(v->mode_dropdown, mode_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    v->swing_btn = create_icon_button(page, RI_SWING, 66, 117, 112, 42, true, &v->swing_icon);
    lv_obj_add_event_cb(v->swing_btn, swing_event_cb, LV_EVENT_CLICKED, NULL);

    /* Center: temperature is the dominant element and owns vertical adjustment. */
    v->temp_label = lv_label_create(page);
    lv_obj_set_width(v->temp_label, 260);
    lv_obj_set_style_text_align(v->temp_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(v->temp_label, UI_TEMP_FONT, 0);
    lv_obj_set_style_text_color(v->temp_label, COLOR_FG, 0);
    lv_obj_set_pos(v->temp_label, 190, 10);
    lv_obj_clear_flag(v->temp_label, LV_OBJ_FLAG_CLICKABLE);

    v->temp_gesture = lv_obj_create(page);
    lv_obj_remove_style_all(v->temp_gesture);
    lv_obj_set_pos(v->temp_gesture, 190, 4);
    lv_obj_set_size(v->temp_gesture, 260, 112);
    lv_obj_set_style_bg_opa(v->temp_gesture, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(v->temp_gesture, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(v->temp_gesture, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(v->temp_gesture, temp_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(v->temp_gesture, temp_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(v->temp_gesture, temp_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(v->temp_gesture, temp_event_cb, LV_EVENT_PRESS_LOST, NULL);

    /* Fan value only appears while dragging. */
    v->fan_value_label = lv_label_create(page);
    lv_obj_set_width(v->fan_value_label, 44);
    lv_obj_set_pos(v->fan_value_label, 298, 111);
    lv_obj_set_style_text_align(v->fan_value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(v->fan_value_label, UI_TEXT_FONT, 0);
    lv_obj_set_style_text_color(v->fan_value_label, COLOR_FG, 0);
    lv_obj_clear_flag(v->fan_value_label, LV_OBJ_FLAG_CLICKABLE);

    v->fan_slider = lv_slider_create(page);
    lv_obj_set_pos(v->fan_slider, 218, 140);
    lv_obj_set_size(v->fan_slider, 212, 10);
    lv_slider_set_range(v->fan_slider, FAN_MIN, FAN_MAX);
    lv_slider_set_value(v->fan_slider, s_fan_speed, LV_ANIM_OFF);
    lv_obj_set_style_radius(v->fan_slider, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(v->fan_slider, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(v->fan_slider, LV_OPA_55, LV_PART_MAIN);
    lv_obj_set_style_bg_color(v->fan_slider, COLOR_FG, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(v->fan_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(v->fan_slider, COLOR_FG, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(v->fan_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(v->fan_slider, 4, LV_PART_KNOB);
    lv_obj_set_style_bg_color(v->fan_slider, COLOR_DISABLED_BR, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(v->fan_slider, COLOR_DIM, LV_PART_INDICATOR | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(v->fan_slider, COLOR_DIM, LV_PART_KNOB | LV_STATE_DISABLED);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(v->fan_slider, fan_event_cb, LV_EVENT_PRESS_LOST, NULL);

    v->fan_auto_label = lv_label_create(page);
    lv_label_set_text(v->fan_auto_label, "AUTO");
    lv_obj_set_style_text_font(v->fan_auto_label, UI_TEXT_FONT, 0);
    lv_obj_set_style_text_color(v->fan_auto_label, COLOR_FG, 0);
    lv_obj_set_width(v->fan_auto_label, 212);
    lv_obj_set_style_text_align(v->fan_auto_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(v->fan_auto_label, 218, 132);
    lv_obj_add_flag(v->fan_auto_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(v->fan_auto_label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(v->fan_auto_label, fan_auto_event_cb, LV_EVENT_CLICKED, NULL);

    /* Right column follows the remote page's rounded gradient button language. */
    const int32_t right_x[2] = { 470, 552 };
    const int32_t right_y[2] = { 31, 95 };
    for (int i = 0; i < 4; ++i) {
        int col = i & 1;
        int row = i >> 1;
        v->feature_btn[i] = create_text_button(page,
                                               s_feature_names[i],
                                               right_x[col],
                                               right_y[row],
                                               72,
                                               46);
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
    s_fan_value_visible = false;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    s_pager = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_pager);
    lv_obj_set_size(s_pager, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_pager, 0, 0);
    lv_obj_set_style_bg_color(s_pager, COLOR_BG, 0);
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

    /* Edge clones preserve the continuous four-page loop. */
    build_placeholder(s_pager, s_pages[UI_DEVICE_PAGE_COUNT - 1].title);
    build_aircon_page(s_pager);
    build_placeholder(s_pager, s_pages[1].title);
    build_placeholder(s_pager, s_pages[2].title);
    build_placeholder(s_pager, s_pages[3].title);
    build_aircon_page(s_pager);

    lv_obj_add_event_cb(s_pager, pager_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_scroll_to_x(s_pager, UI_SCREEN_W, LV_ANIM_OFF);
    return s_root;
}

void ui_page_devices_stop(void)
{
    s_root = NULL;
    s_pager = NULL;
    s_wrapping = false;
    s_updating_views = false;
    s_aircon_view_count = 0;
    s_fan_value_visible = false;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}
