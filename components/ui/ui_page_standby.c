#include "ui_page_standby.h"

#include <time.h>

#define UI_SCREEN_W              640
#define UI_SCREEN_H              172

#define CLOCK_GREEN              lv_color_hex(0x39FF14)
#define CLOCK_BG                 lv_color_hex(0x000000)
#define CLOCK_HINT               lv_color_hex(0x777777)
#define CLOCK_DOT_OFF            lv_color_hex(0x2A2A2A)

#define PIXEL_SIZE               14
#define PIXEL_STEP               18
#define DIGIT_W                  ((5 * PIXEL_STEP) - (PIXEL_STEP - PIXEL_SIZE))
#define DIGIT_H                  ((7 * PIXEL_STEP) - (PIXEL_STEP - PIXEL_SIZE))
#define DIGIT_GAP                12
#define COLON_GAP                22
#define COLON_W                  PIXEL_SIZE

static lv_timer_t *s_clock_timer = NULL;
static lv_timer_t *s_hint_timer = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_hint_left = NULL;
static lv_obj_t *s_hint_right = NULL;
static ui_standby_event_cb_t s_event_cb = NULL;
static void *s_event_user_data = NULL;
static int s_last_hour = -1;
static int s_last_minute = -1;
static bool s_gesture_seen = false;

static const uint8_t DIGIT_ROWS[10][7] = {
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, /* 0 */
    { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E }, /* 1 */
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, /* 2 */
    { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E }, /* 3 */
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, /* 4 */
    { 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E }, /* 5 */
    { 0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E }, /* 6 */
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }, /* 7 */
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, /* 8 */
    { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E }, /* 9 */
};

static void delete_timer(lv_timer_t **timer)
{
    if (*timer != NULL) {
        lv_timer_delete(*timer);
        *timer = NULL;
    }
}

void ui_page_standby_stop(void)
{
    delete_timer(&s_clock_timer);
    delete_timer(&s_hint_timer);
    s_content = NULL;
    s_hint_left = NULL;
    s_hint_right = NULL;
    s_last_hour = -1;
    s_last_minute = -1;
}

static lv_obj_t *create_pixel(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *pixel = lv_obj_create(parent);
    lv_obj_remove_style_all(pixel);
    lv_obj_set_size(pixel, PIXEL_SIZE, PIXEL_SIZE);
    lv_obj_set_pos(pixel, x, y);
    lv_obj_set_style_bg_color(pixel, CLOCK_GREEN, 0);
    lv_obj_set_style_bg_opa(pixel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pixel, 2, 0);
    lv_obj_clear_flag(pixel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(pixel, LV_OBJ_FLAG_SCROLLABLE);
    return pixel;
}

static void draw_digit(lv_obj_t *parent, uint8_t digit, int32_t x, int32_t y)
{
    if (digit > 9) {
        return;
    }

    for (int row = 0; row < 7; ++row) {
        uint8_t bits = DIGIT_ROWS[digit][row];
        for (int col = 0; col < 5; ++col) {
            if (bits & (1u << (4 - col))) {
                create_pixel(parent,
                             x + col * PIXEL_STEP,
                             y + row * PIXEL_STEP);
            }
        }
    }
}

static void draw_colon(lv_obj_t *parent, int32_t x, int32_t y)
{
    create_pixel(parent, x, y + 2 * PIXEL_STEP);
    create_pixel(parent, x, y + 4 * PIXEL_STEP);
}

static bool read_clock(int *hour12, int *minute)
{
    time_t now = time(NULL);
    struct tm local_tm;

    if (now <= 0 || localtime_r(&now, &local_tm) == NULL) {
        *hour12 = 12;
        *minute = 0;
        return false;
    }

    int h = local_tm.tm_hour % 12;
    if (h == 0) {
        h = 12;
    }

    *hour12 = h;
    *minute = local_tm.tm_min;
    return true;
}

static void redraw_clock(void)
{
    if (s_content == NULL) {
        return;
    }

    int hour12 = 12;
    int minute = 0;
    read_clock(&hour12, &minute);

    if (hour12 == s_last_hour && minute == s_last_minute) {
        return;
    }

    s_last_hour = hour12;
    s_last_minute = minute;
    lv_obj_clean(s_content);

    const int hour_digits = (hour12 >= 10) ? 2 : 1;
    const int hour_w = hour_digits * DIGIT_W + (hour_digits - 1) * DIGIT_GAP;
    const int minute_w = 2 * DIGIT_W + DIGIT_GAP;
    const int total_w = hour_w + COLON_GAP + COLON_W + COLON_GAP + minute_w;
    const int x0 = (UI_SCREEN_W - total_w) / 2;
    const int y0 = 13;

    int x = x0;

    if (hour_digits == 2) {
        draw_digit(s_content, (uint8_t)(hour12 / 10), x, y0);
        x += DIGIT_W + DIGIT_GAP;
    }

    draw_digit(s_content, (uint8_t)(hour12 % 10), x, y0);
    x += DIGIT_W + COLON_GAP;

    draw_colon(s_content, x, y0);
    x += COLON_W + COLON_GAP;

    draw_digit(s_content, (uint8_t)(minute / 10), x, y0);
    x += DIGIT_W + DIGIT_GAP;
    draw_digit(s_content, (uint8_t)(minute % 10), x, y0);
}

static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    redraw_clock();
}

static void hide_hint_timer_cb(lv_timer_t *timer)
{
    if (s_hint_left != NULL) {
        lv_obj_add_flag(s_hint_left, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_hint_right != NULL) {
        lv_obj_add_flag(s_hint_right, LV_OBJ_FLAG_HIDDEN);
    }

    lv_timer_delete(timer);
    s_hint_timer = NULL;
}

static void add_page_dots(lv_obj_t *parent, ui_standby_view_t active)
{
    const int dot_size = 7;
    const int gap = 12;
    const int total = 3 * dot_size + 2 * gap;
    const int x0 = (UI_SCREEN_W - total) / 2;
    const int y = UI_SCREEN_H - 12;

    for (int i = 0; i < 3; ++i) {
        lv_obj_t *dot = lv_obj_create(parent);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, dot_size, dot_size);
        lv_obj_set_pos(dot, x0 + i * (dot_size + gap), y);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot,
                                  i == (int)active ? CLOCK_GREEN : CLOCK_DOT_OFF,
                                  0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }
}

static void add_swipe_hint(lv_obj_t *parent, bool visible)
{
    s_hint_left = lv_label_create(parent);
    lv_label_set_text(s_hint_left, "<");
    lv_obj_set_style_text_font(s_hint_left, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_hint_left, CLOCK_HINT, 0);
    lv_obj_align(s_hint_left, LV_ALIGN_LEFT_MID, 10, -3);
    lv_obj_clear_flag(s_hint_left, LV_OBJ_FLAG_CLICKABLE);

    s_hint_right = lv_label_create(parent);
    lv_label_set_text(s_hint_right, ">");
    lv_obj_set_style_text_font(s_hint_right, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_hint_right, CLOCK_HINT, 0);
    lv_obj_align(s_hint_right, LV_ALIGN_RIGHT_MID, -10, -3);
    lv_obj_clear_flag(s_hint_right, LV_OBJ_FLAG_CLICKABLE);

    if (!visible) {
        lv_obj_add_flag(s_hint_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_hint_right, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    s_hint_timer = lv_timer_create(hide_hint_timer_cb, 1000, NULL);
}

static void add_placeholder(lv_obj_t *parent, const char *name)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, name);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x7A7A7A), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -8);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *subtitle = lv_label_create(parent);
    lv_label_set_text(subtitle, "PLACEHOLDER");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x383838), 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 16);
    lv_obj_clear_flag(subtitle, LV_OBJ_FLAG_CLICKABLE);
}

static void standby_input_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        s_gesture_seen = false;
        return;
    }

    if (code == LV_EVENT_GESTURE) {
        s_gesture_seen = true;
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

        if (s_event_cb == NULL) {
            return;
        }

        if (dir == LV_DIR_TOP) {
            s_event_cb(UI_STANDBY_EVENT_OPEN_HOME, s_event_user_data);
        }
        else if (dir == LV_DIR_LEFT) {
            s_event_cb(UI_STANDBY_EVENT_NEXT, s_event_user_data);
        }
        else if (dir == LV_DIR_RIGHT) {
            s_event_cb(UI_STANDBY_EVENT_PREVIOUS, s_event_user_data);
        }
        return;
    }

    if (code == LV_EVENT_SHORT_CLICKED && !s_gesture_seen && s_event_cb != NULL) {
        s_event_cb(UI_STANDBY_EVENT_OPEN_HOME, s_event_user_data);
    }
}

void ui_page_standby_show(ui_standby_view_t view,
                          bool show_swipe_hint,
                          ui_standby_event_cb_t event_cb,
                          void *event_user_data)
{
    ui_page_standby_stop();

    s_event_cb = event_cb;
    s_event_user_data = event_user_data;

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_size(screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(screen, CLOCK_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_center(root);
    lv_obj_set_style_bg_color(root, CLOCK_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(root, standby_input_cb, LV_EVENT_SHORT_CLICKED, NULL);

    s_content = lv_obj_create(root);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_size(s_content, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_content, 0, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    if (view == UI_STANDBY_CLOCK) {
        redraw_clock();
        s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    }
    else if (view == UI_STANDBY_WEATHER) {
        add_placeholder(s_content, "WEATHER");
    }
    else {
        add_placeholder(s_content, "CALENDAR");
    }

    add_page_dots(root, view);
    add_swipe_hint(root, show_swipe_hint);
}
