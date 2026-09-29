#include "ui_page_remote.h"

#include <stdbool.h>
#include <stdint.h>

#define REMOTE_W                 640
#define REMOTE_H                 172
#define CIRCLE_SIZE               52
#define TOUCH_X                  128
#define TOUCH_Y                   20
#define TOUCH_W                  280
#define TOUCH_H                  132
#define TOUCH_RADIUS              24
#define TOUCH_SWIPE_THRESHOLD     34
#define TOUCH_TAP_SLOP            12

#define COLOR_BG        lv_color_hex(0x000000)
#define COLOR_PANEL     lv_color_hex(0x0A101A)
#define COLOR_BUTTON    lv_color_hex(0x111824)
#define COLOR_BUTTON_2  lv_color_hex(0x171F2D)
#define COLOR_BORDER    lv_color_hex(0x526581)
#define COLOR_ICON      lv_color_hex(0xEAF2FF)
#define COLOR_POWER     lv_color_hex(0xFF604F)
#define COLOR_TOUCH     lv_color_hex(0x142238)
#define COLOR_TOUCH_2   lv_color_hex(0x0B1422)
#define COLOR_PRESSED   lv_color_hex(0x24344D)

static ui_remote_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static ui_remote_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static lv_point_t s_touch_press = {0, 0};
static bool s_touch_active = false;

typedef struct {
    ui_remote_action_t action;
    const char *symbol;
    lv_color_t color;
} remote_button_data_t;

static remote_button_data_t s_power   = { UI_REMOTE_ACTION_POWER,       LV_SYMBOL_POWER,    COLOR_POWER };
static remote_button_data_t s_input   = { UI_REMOTE_ACTION_INPUT,       LV_SYMBOL_VIDEO,    COLOR_ICON };
static remote_button_data_t s_home    = { UI_REMOTE_ACTION_HOME,        LV_SYMBOL_HOME,     COLOR_ICON };
static remote_button_data_t s_back    = { UI_REMOTE_ACTION_BACK,        LV_SYMBOL_LEFT,     COLOR_ICON };
static remote_button_data_t s_setup   = { UI_REMOTE_ACTION_SETTINGS,    LV_SYMBOL_SETTINGS, COLOR_ICON };
static remote_button_data_t s_display = { UI_REMOTE_ACTION_DISPLAY,     LV_SYMBOL_IMAGE,    COLOR_ICON };
static remote_button_data_t s_vol_up  = { UI_REMOTE_ACTION_VOLUME_UP,   LV_SYMBOL_PLUS,     COLOR_ICON };
static remote_button_data_t s_vol_dn  = { UI_REMOTE_ACTION_VOLUME_DOWN, LV_SYMBOL_MINUS,    COLOR_ICON };

static int32_t iabs32(int32_t value)
{
    return value < 0 ? -value : value;
}

static void note_activity(void)
{
    if (s_activity_cb != NULL) {
        s_activity_cb(s_activity_user_data);
    }
}

static void emit_action(ui_remote_action_t action, int32_t value)
{
    note_activity();
    if (s_action_cb != NULL) {
        s_action_cb(action, value, s_action_user_data);
    }
}

static void button_event_cb(lv_event_t *e)
{
    lv_obj_t *button = lv_event_get_current_target(e);
    remote_button_data_t *data = (remote_button_data_t *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        note_activity();
        lv_obj_set_style_bg_color(button, COLOR_PRESSED, 0);
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_bg_color(button, COLOR_BUTTON, 0);
    }

    if (code == LV_EVENT_CLICKED && data != NULL) {
        emit_action(data->action, 0);
    }
}

static lv_obj_t *create_icon_label(lv_obj_t *parent,
                                   const char *symbol,
                                   lv_color_t color,
                                   int32_t font_size_hint)
{
    (void)font_size_hint;
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

static lv_obj_t *create_circle_button(lv_obj_t *parent,
                                      int32_t x,
                                      int32_t y,
                                      remote_button_data_t *data)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, CIRCLE_SIZE, CIRCLE_SIZE);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, COLOR_BUTTON, 0);
    lv_obj_set_style_bg_grad_color(button, COLOR_BUTTON_2, 0);
    lv_obj_set_style_bg_grad_dir(button, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, COLOR_BORDER, 0);
    lv_obj_set_style_border_opa(button, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(button, 10, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(button, COLOR_BORDER, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

    create_icon_label(button, data->symbol, data->color, 28);

    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_PRESSED, data);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_RELEASED, data);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_PRESS_LOST, data);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_CLICKED, data);
    return button;
}

static void touchpad_event_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) return;

    lv_obj_t *pad = lv_event_get_current_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_touch_press);
        s_touch_active = true;
        note_activity();
        lv_obj_set_style_border_color(pad, lv_color_hex(0x7897C8), 0);
        return;
    }

    if (!s_touch_active) return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    int32_t dx = point.x - s_touch_press.x;
    int32_t dy = point.y - s_touch_press.y;

    if (code == LV_EVENT_PRESSING) {
        note_activity();
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        s_touch_active = false;
        lv_obj_set_style_border_color(pad, COLOR_BORDER, 0);

        int32_t ax = iabs32(dx);
        int32_t ay = iabs32(dy);

        if (ax <= TOUCH_TAP_SLOP && ay <= TOUCH_TAP_SLOP) {
            emit_action(UI_REMOTE_ACTION_OK, 0);
            return;
        }

        if (ax >= TOUCH_SWIPE_THRESHOLD || ay >= TOUCH_SWIPE_THRESHOLD) {
            if (ax > ay) {
                emit_action(dx > 0 ? UI_REMOTE_ACTION_SWIPE_RIGHT
                                   : UI_REMOTE_ACTION_SWIPE_LEFT,
                            ax);
            } else {
                emit_action(dy > 0 ? UI_REMOTE_ACTION_SWIPE_DOWN
                                   : UI_REMOTE_ACTION_SWIPE_UP,
                            ay);
            }
        }
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        s_touch_active = false;
        lv_obj_set_style_border_color(pad, COLOR_BORDER, 0);
    }
}

static lv_obj_t *create_touchpad(lv_obj_t *parent)
{
    lv_obj_t *pad = lv_obj_create(parent);
    lv_obj_remove_style_all(pad);
    lv_obj_set_pos(pad, TOUCH_X, TOUCH_Y);
    lv_obj_set_size(pad, TOUCH_W, TOUCH_H);
    lv_obj_set_style_radius(pad, TOUCH_RADIUS, 0);
    lv_obj_set_style_bg_color(pad, COLOR_TOUCH, 0);
    lv_obj_set_style_bg_grad_color(pad, COLOR_TOUCH_2, 0);
    lv_obj_set_style_bg_grad_dir(pad, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pad, 1, 0);
    lv_obj_set_style_border_color(pad, COLOR_BORDER, 0);
    lv_obj_set_style_border_opa(pad, LV_OPA_80, 0);
    lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(pad, touchpad_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(pad, touchpad_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(pad, touchpad_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(pad, touchpad_event_cb, LV_EVENT_PRESS_LOST, NULL);
    return pad;
}

static lv_obj_t *create_volume_rocker(lv_obj_t *parent)
{
    const int32_t x = 484;
    const int32_t y = 100;
    const int32_t w = 116;
    const int32_t h = 52;

    lv_obj_t *rocker = lv_obj_create(parent);
    lv_obj_remove_style_all(rocker);
    lv_obj_set_pos(rocker, x, y);
    lv_obj_set_size(rocker, w, h);
    lv_obj_set_style_radius(rocker, 24, 0);
    lv_obj_set_style_bg_color(rocker, COLOR_BUTTON, 0);
    lv_obj_set_style_bg_grad_color(rocker, COLOR_BUTTON_2, 0);
    lv_obj_set_style_bg_grad_dir(rocker, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(rocker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(rocker, 1, 0);
    lv_obj_set_style_border_color(rocker, COLOR_BORDER, 0);
    lv_obj_set_style_border_opa(rocker, LV_OPA_70, 0);
    lv_obj_clear_flag(rocker, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *divider = lv_obj_create(rocker);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, 1, h - 12);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(divider, COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_50, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);

    /* Requested order: volume + on the left, volume - on the right. */
    lv_obj_t *plus = lv_obj_create(rocker);
    lv_obj_remove_style_all(plus);
    lv_obj_set_pos(plus, 0, 0);
    lv_obj_set_size(plus, w / 2, h);
    lv_obj_set_style_bg_opa(plus, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(plus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(plus, LV_OBJ_FLAG_SCROLLABLE);
    create_icon_label(plus, s_vol_up.symbol, s_vol_up.color, 26);
    lv_obj_add_event_cb(plus, button_event_cb, LV_EVENT_CLICKED, &s_vol_up);

    lv_obj_t *minus = lv_obj_create(rocker);
    lv_obj_remove_style_all(minus);
    lv_obj_set_pos(minus, w / 2, 0);
    lv_obj_set_size(minus, w - (w / 2), h);
    lv_obj_set_style_bg_opa(minus, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(minus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(minus, LV_OBJ_FLAG_SCROLLABLE);
    create_icon_label(minus, s_vol_dn.symbol, s_vol_dn.color, 26);
    lv_obj_add_event_cb(minus, button_event_cb, LV_EVENT_CLICKED, &s_vol_dn);

    return rocker;
}

void ui_page_remote_build(lv_obj_t *parent,
                          ui_remote_activity_cb_t activity_cb,
                          void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_touch_active = false;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, REMOTE_W, REMOTE_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* x < 56 is intentionally kept clear for ui_page_feature's global back rail. */
    create_circle_button(root, 64, 22, &s_power);
    create_circle_button(root, 64, 98, &s_input);

    create_touchpad(root);

    create_circle_button(root, 420, 22, &s_home);
    create_circle_button(root, 420, 98, &s_back);

    create_circle_button(root, 484, 22, &s_setup);
    create_circle_button(root, 548, 22, &s_display);
    create_volume_rocker(root);
}

void ui_page_remote_stop(void)
{
    s_touch_active = false;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}

void ui_page_remote_set_action_cb(ui_remote_action_cb_t cb, void *user_data)
{
    s_action_cb = cb;
    s_action_user_data = user_data;
}
