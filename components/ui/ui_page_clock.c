#include "ui_page_clock.h"

#include <time.h>

#define UI_SCREEN_W              640
#define UI_SCREEN_H              172

#define CLOCK_GREEN              lv_color_hex(0x39FF14)
#define PIXEL_SIZE               14
#define PIXEL_STEP               18
#define DIGIT_W                  ((5 * PIXEL_STEP) - (PIXEL_STEP - PIXEL_SIZE))
#define DIGIT_H                  ((7 * PIXEL_STEP) - (PIXEL_STEP - PIXEL_SIZE))
#define DIGIT_GAP                12
#define COLON_GAP                22
#define COLON_W                  PIXEL_SIZE

static lv_timer_t *s_clock_timer = NULL;
static lv_obj_t *s_parent = NULL;
static int s_last_hour = -1;
static int s_last_minute = -1;

static const uint8_t DIGIT_ROWS[10][7] = {
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E },
    { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F },
    { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E },
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 },
    { 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E },
    { 0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E },
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E },
    { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E },
};

void ui_page_clock_stop(void)
{
    if (s_clock_timer != NULL) {
        lv_timer_delete(s_clock_timer);
        s_clock_timer = NULL;
    }

    s_parent = NULL;
    s_last_hour = -1;
    s_last_minute = -1;
}

static void create_pixel(int32_t x, int32_t y)
{
    lv_obj_t *pixel = lv_obj_create(s_parent);
    lv_obj_remove_style_all(pixel);
    lv_obj_set_size(pixel, PIXEL_SIZE, PIXEL_SIZE);
    lv_obj_set_pos(pixel, x, y);
    lv_obj_set_style_bg_color(pixel, CLOCK_GREEN, 0);
    lv_obj_set_style_bg_opa(pixel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pixel, 2, 0);
    lv_obj_clear_flag(pixel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(pixel, LV_OBJ_FLAG_SCROLLABLE);
}

static void draw_digit(uint8_t digit, int32_t x, int32_t y)
{
    if (digit > 9) {
        return;
    }

    for (int row = 0; row < 7; ++row) {
        uint8_t bits = DIGIT_ROWS[digit][row];
        for (int col = 0; col < 5; ++col) {
            if (bits & (1u << (4 - col))) {
                create_pixel(x + col * PIXEL_STEP,
                             y + row * PIXEL_STEP);
            }
        }
    }
}

static void draw_colon(int32_t x, int32_t y)
{
    create_pixel(x, y + 2 * PIXEL_STEP);
    create_pixel(x, y + 4 * PIXEL_STEP);
}

static void read_clock(int *hour12, int *minute)
{
    time_t now = time(NULL);
    struct tm local_tm;

    if (now <= 0 || localtime_r(&now, &local_tm) == NULL) {
        *hour12 = 12;
        *minute = 0;
        return;
    }

    int h = local_tm.tm_hour % 12;
    *hour12 = (h == 0) ? 12 : h;
    *minute = local_tm.tm_min;
}

static void redraw_clock(void)
{
    if (s_parent == NULL) {
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
    lv_obj_clean(s_parent);

    const int hour_digits = (hour12 >= 10) ? 2 : 1;
    const int hour_w = hour_digits * DIGIT_W + (hour_digits - 1) * DIGIT_GAP;
    const int minute_w = 2 * DIGIT_W + DIGIT_GAP;
    const int total_w = hour_w + COLON_GAP + COLON_W + COLON_GAP + minute_w;
    const int x0 = (UI_SCREEN_W - total_w) / 2;
    const int y0 = (UI_SCREEN_H - DIGIT_H) / 2;

    int x = x0;

    if (hour_digits == 2) {
        draw_digit((uint8_t)(hour12 / 10), x, y0);
        x += DIGIT_W + DIGIT_GAP;
    }

    draw_digit((uint8_t)(hour12 % 10), x, y0);
    x += DIGIT_W + COLON_GAP;

    draw_colon(x, y0);
    x += COLON_W + COLON_GAP;

    draw_digit((uint8_t)(minute / 10), x, y0);
    x += DIGIT_W + DIGIT_GAP;
    draw_digit((uint8_t)(minute % 10), x, y0);
}

static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    redraw_clock();
}

void ui_page_clock_build(lv_obj_t *parent)
{
    ui_page_clock_stop();

    s_parent = parent;
    s_last_hour = -1;
    s_last_minute = -1;

    redraw_clock();
    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
}
