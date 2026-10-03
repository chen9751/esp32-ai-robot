#include "ui_page_remote.h"
#include "ui_remote_icons.h"
#if defined(ESP_PLATFORM)
#include "board.h"
#endif

#include <stdbool.h>
#include <stdint.h>

#define REMOTE_W                 640
#define REMOTE_H                 172
#define CIRCLE_SIZE               52
#define TOUCH_X                  128
#define TOUCH_Y                   20
#define TOUCH_W                  258
#define TOUCH_H                  132
#define TOUCH_RADIUS              24
#define TOUCH_SWIPE_THRESHOLD     34
#define TOUCH_TAP_SLOP            12
#define REMOTE_ICON_COUNT           8
#define ICON_ROTATION_MS          240

#define COLOR_BG        lv_color_hex(0x000000)
#define COLOR_BUTTON    lv_color_hex(0x111824)
#define COLOR_BUTTON_2  lv_color_hex(0x171F2D)
#define COLOR_BORDER    lv_color_hex(0x526581)
#define COLOR_ICON_HEX             0xEAF2FFu
#define COLOR_POWER_HEX            0xFF604Fu
#define COLOR_TOUCH     lv_color_hex(0x142238)
#define COLOR_TOUCH_2   lv_color_hex(0x0B1422)
#define COLOR_PRESSED   lv_color_hex(0x24344D)

static ui_remote_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static ui_remote_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static lv_point_t s_touch_press = {0, 0};
static bool s_touch_active = false;
static bool s_icons_ccw90 = false;
static int16_t s_icon_angle = 0;
static lv_obj_t *s_icons[REMOTE_ICON_COUNT] = {0};
static uint8_t s_icon_count = 0;
#if defined(ESP_PLATFORM)
static lv_timer_t *s_orientation_timer = NULL;
static board_orientation_t s_last_orientation = BOARD_ORIENTATION_UNKNOWN;
#endif

typedef struct {
    ui_remote_action_t action;
    ui_remote_icon_t icon;
    uint32_t color_hex;
} remote_button_data_t;

/* lv_color_hex() is a runtime LVGL function in v9.x, so file-scope data stores
 * compile-time RGB integers and converts them when the icon object is created. */
static remote_button_data_t s_power   = { UI_REMOTE_ACTION_POWER,       UI_REMOTE_ICON_POWER,    COLOR_POWER_HEX };
static remote_button_data_t s_input   = { UI_REMOTE_ACTION_INPUT,       UI_REMOTE_ICON_INPUT,    COLOR_ICON_HEX };
static remote_button_data_t s_home    = { UI_REMOTE_ACTION_HOME,        UI_REMOTE_ICON_HOME,     COLOR_ICON_HEX };
static remote_button_data_t s_back    = { UI_REMOTE_ACTION_BACK,        UI_REMOTE_ICON_BACK,     COLOR_ICON_HEX };
static remote_button_data_t s_setup   = { UI_REMOTE_ACTION_SETTINGS,    UI_REMOTE_ICON_SETTINGS, COLOR_ICON_HEX };
static remote_button_data_t s_display = { UI_REMOTE_ACTION_DISPLAY,     UI_REMOTE_ICON_DISPLAY,  COLOR_ICON_HEX };
static remote_button_data_t s_vol_up  = { UI_REMOTE_ACTION_VOLUME_UP,   UI_REMOTE_ICON_ADD,      COLOR_ICON_HEX };
static remote_button_data_t s_vol_dn  = { UI_REMOTE_ACTION_VOLUME_DOWN, UI_REMOTE_ICON_SUBTRACT, COLOR_ICON_HEX };

static int32_t iabs32(int32_t value) { return value < 0 ? -value : value; }

static void note_activity(void)
{
    if (s_activity_cb != NULL) s_activity_cb(s_activity_user_data);
}

static void emit_action(ui_remote_action_t action, int32_t value)
{
    note_activity();
    if (s_action_cb != NULL) s_action_cb(action, value, s_action_user_data);
}

static void icon_rotation_exec_cb(void *obj, int32_t angle)
{
    lv_image_set_rotation((lv_obj_t *)obj, angle);
}

static void register_icon(lv_obj_t *image)
{
    if (image == NULL) return;
    if (s_icon_count < REMOTE_ICON_COUNT) s_icons[s_icon_count++] = image;
    lv_image_set_rotation(image, s_icon_angle);
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
    if (code == LV_EVENT_CLICKED && data != NULL) emit_action(data->action, 0);
}

static lv_obj_t *create_icon(lv_obj_t *parent, ui_remote_icon_t icon, uint32_t color_hex)
{
    lv_obj_t *image = ui_remote_icon_create(parent, icon, lv_color_hex(color_hex));
    lv_obj_center(image);
    register_icon(image);
    return image;
}

static lv_obj_t *create_circle_button(lv_obj_t *parent, int32_t x, int32_t y, remote_button_data_t *data)
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
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    create_icon(button, data->icon, data->color_hex);
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
        int32_t ax = iabs32(dx), ay = iabs32(dy);
        if (ax <= TOUCH_TAP_SLOP && ay <= TOUCH_TAP_SLOP) {
            emit_action(UI_REMOTE_ACTION_OK, 0);
        } else if (ax >= TOUCH_SWIPE_THRESHOLD || ay >= TOUCH_SWIPE_THRESHOLD) {
            if (ax > ay) emit_action(dx > 0 ? UI_REMOTE_ACTION_SWIPE_RIGHT : UI_REMOTE_ACTION_SWIPE_LEFT, ax);
            else emit_action(dy > 0 ? UI_REMOTE_ACTION_SWIPE_DOWN : UI_REMOTE_ACTION_SWIPE_UP, ay);
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

static lv_obj_t *create_volume_half(lv_obj_t *rocker, int32_t x, int32_t w, remote_button_data_t *data)
{
    lv_obj_t *button = lv_obj_create(rocker);
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, 0);
    lv_obj_set_size(button, w, 52);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    create_icon(button, data->icon, data->color_hex);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_CLICKED, data);
    return button;
}

static lv_obj_t *create_volume_rocker(lv_obj_t *parent)
{
    const int32_t x = 462, y = 100, w = 150, h = 52;
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

    /* User-approved order: volume + on the left, volume - on the right. */
    create_volume_half(rocker, 0, w / 2, &s_vol_up);
    create_volume_half(rocker, w / 2, w - (w / 2), &s_vol_dn);
    return rocker;
}

static void set_icon_angle(int16_t angle, bool animate)
{
    while (angle < 0) angle += 3600;
    angle %= 3600;
    if (s_icon_angle == angle && s_icon_count > 0) return;

    int16_t old_angle = s_icon_angle;
    s_icon_angle = angle;
    s_icons_ccw90 = (angle == 2700);

    for (uint8_t i = 0; i < s_icon_count; i++) {
        lv_obj_t *icon = s_icons[i];
        if (icon == NULL) continue;
        lv_anim_delete(icon, icon_rotation_exec_cb);

        if (!animate) {
            lv_image_set_rotation(icon, angle);
            continue;
        }

        int32_t start = old_angle;
        int32_t end = angle;
        int32_t delta = end - start;
        if (delta > 1800) end -= 3600;
        else if (delta < -1800) end += 3600;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, icon);
        lv_anim_set_exec_cb(&a, icon_rotation_exec_cb);
        lv_anim_set_duration(&a, ICON_ROTATION_MS);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_values(&a, start, end);
        lv_anim_start(&a);
    }
}

#if defined(ESP_PLATFORM)
static void orientation_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    board_orientation_t orientation = BOARD_ORIENTATION_UNKNOWN;
    if (board_imu_get_orientation(&orientation) != ESP_OK ||
        orientation == BOARD_ORIENTATION_UNKNOWN ||
        orientation == s_last_orientation) {
        return;
    }

    s_last_orientation = orientation;
    int16_t angle = 0;
    switch (orientation) {
        case BOARD_ORIENTATION_PORTRAIT_RIGHT:      angle = 900;  break;
        case BOARD_ORIENTATION_LANDSCAPE_INVERTED: angle = 1800; break;
        case BOARD_ORIENTATION_PORTRAIT_LEFT:       angle = 2700; break;
        case BOARD_ORIENTATION_LANDSCAPE:
        default:                                    angle = 0;    break;
    }
    set_icon_angle(angle, true);
}
#endif

void ui_page_remote_build(lv_obj_t *parent, ui_remote_activity_cb_t activity_cb, void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_touch_active = false;
    s_icon_count = 0;
    for (uint8_t i = 0; i < REMOTE_ICON_COUNT; i++) s_icons[i] = NULL;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, REMOTE_W, REMOTE_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* Geometry intentionally unchanged by orientation: only icon images rotate. */
    create_circle_button(root, 64, 22, &s_power);
    create_circle_button(root, 64, 98, &s_input);
    create_touchpad(root);
    create_circle_button(root, 398, 22, &s_home);
    create_circle_button(root, 398, 98, &s_back);
    create_circle_button(root, 474, 22, &s_setup);
    create_circle_button(root, 549, 22, &s_display);
    create_volume_rocker(root);

#if defined(ESP_PLATFORM)
    s_last_orientation = BOARD_ORIENTATION_UNKNOWN;
    if (s_orientation_timer != NULL) {
        lv_timer_delete(s_orientation_timer);
    }
    s_orientation_timer = lv_timer_create(orientation_timer_cb, 200, NULL);
    orientation_timer_cb(s_orientation_timer);
#endif
}

void ui_page_remote_stop(void)
{
#if defined(ESP_PLATFORM)
    if (s_orientation_timer != NULL) {
        lv_timer_delete(s_orientation_timer);
        s_orientation_timer = NULL;
    }
    s_last_orientation = BOARD_ORIENTATION_UNKNOWN;
#endif
    s_touch_active = false;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
    s_icon_count = 0;
    for (uint8_t i = 0; i < REMOTE_ICON_COUNT; i++) s_icons[i] = NULL;
}

void ui_page_remote_set_action_cb(ui_remote_action_cb_t cb, void *user_data)
{
    s_action_cb = cb;
    s_action_user_data = user_data;
}

void ui_page_remote_set_icons_ccw90(bool ccw90, bool animate)
{
    set_icon_angle(ccw90 ? 2700 : 0, animate);
}
