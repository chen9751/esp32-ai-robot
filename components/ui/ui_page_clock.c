#include "ui_page_clock.h"

#include <stddef.h>
#include <time.h>

#if defined(ESP_PLATFORM)
#include "board.h"
#endif

#define UI_SCREEN_W              640
#define UI_SCREEN_H              172

#define CLOCK_GREEN              lv_color_hex(0x39FF14)
#define CLOCK_LOW                lv_color_hex(0xFF5A5A)
#define CLOCK_CHARGING           lv_color_hex(0x39FF14)
#define PIXEL_SIZE               14
#define PIXEL_STEP               18
#define DIGIT_W                  ((5 * PIXEL_STEP) - (PIXEL_STEP - PIXEL_SIZE))
#define DIGIT_H                  ((7 * PIXEL_STEP) - (PIXEL_STEP - PIXEL_SIZE))
#define DIGIT_GAP                12
#define COLON_GAP                22
#define COLON_W                  PIXEL_SIZE
#define CLOCK_RECT_POOL_MAX      96

static lv_timer_t *s_clock_timer = NULL;
static lv_obj_t *s_parent = NULL;
static int s_last_hour = -1;
static int s_last_minute = -1;
static ui_clock_battery_state_t s_battery_state = UI_CLOCK_BATTERY_NORMAL;

/*
 * The clock used to delete and recreate dozens of LVGL objects every minute.
 * That looks fine in the web preview but creates avoidable heap churn on a
 * device expected to stay on for weeks. Keep a small object pool instead:
 * page entry allocates up to the high-water mark once, minute updates only
 * reposition/show/hide existing rectangles.
 */
static lv_obj_t *s_rect_pool[CLOCK_RECT_POOL_MAX];
static size_t s_rect_count = 0;
static size_t s_rect_used = 0;

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
    s_rect_count = 0;
    s_rect_used = 0;
    for (size_t i = 0; i < CLOCK_RECT_POOL_MAX; ++i) {
        s_rect_pool[i] = NULL;
    }
}

static lv_obj_t *use_rect(int32_t x,
                          int32_t y,
                          int32_t w,
                          int32_t h,
                          lv_color_t color,
                          int32_t radius)
{
    if (s_parent == NULL || s_rect_used >= CLOCK_RECT_POOL_MAX) {
        return NULL;
    }

    lv_obj_t *obj = NULL;
    if (s_rect_used < s_rect_count) {
        obj = s_rect_pool[s_rect_used];
    }
    else {
        obj = lv_obj_create(s_parent);
        if (obj == NULL) {
            return NULL;
        }
        lv_obj_remove_style_all(obj);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        s_rect_pool[s_rect_count++] = obj;
    }

    ++s_rect_used;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    return obj;
}

static void finish_rect_frame(void)
{
    for (size_t i = s_rect_used; i < s_rect_count; ++i) {
        lv_obj_add_flag(s_rect_pool[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void draw_pixel(int32_t x, int32_t y)
{
    (void)use_rect(x, y, PIXEL_SIZE, PIXEL_SIZE, CLOCK_GREEN, 2);
}

static void draw_status_rect(int32_t x,
                             int32_t y,
                             int32_t w,
                             int32_t h,
                             lv_color_t color)
{
    (void)use_rect(x, y, w, h, color, 0);
}

static void draw_battery_outline(int32_t x, int32_t y, lv_color_t color)
{
    draw_status_rect(x, y, 2, 16, color);
    draw_status_rect(x + 2, y, 22, 2, color);
    draw_status_rect(x + 2, y + 14, 22, 2, color);
    draw_status_rect(x + 24, y, 2, 16, color);
    draw_status_rect(x + 28, y + 4, 3, 8, color);
}

static void draw_low_battery_icon(void)
{
    const int32_t x = 14;
    const int32_t y = 12;

    draw_battery_outline(x, y, CLOCK_LOW);
    draw_status_rect(x + 5, y + 5, 4, 6, CLOCK_LOW);
}

static void draw_charging_icon(void)
{
    const int32_t x = 14;
    const int32_t y = 12;

    draw_battery_outline(x, y, CLOCK_CHARGING);
    draw_status_rect(x + 12, y + 3, 3, 5, CLOCK_CHARGING);
    draw_status_rect(x + 9, y + 7, 6, 3, CLOCK_CHARGING);
    draw_status_rect(x + 10, y + 9, 3, 4, CLOCK_CHARGING);
}

static void draw_battery_status(void)
{
    if (s_battery_state == UI_CLOCK_BATTERY_LOW) {
        draw_low_battery_icon();
    }
    else if (s_battery_state == UI_CLOCK_BATTERY_CHARGING) {
        draw_charging_icon();
    }
}

static void draw_digit(uint8_t digit, int32_t x, int32_t y)
{
    if (digit > 9) {
        return;
    }

    for (int row = 0; row < 7; ++row) {
        const uint8_t bits = DIGIT_ROWS[digit][row];
        for (int col = 0; col < 5; ++col) {
            if (bits & (1u << (4 - col))) {
                draw_pixel(x + col * PIXEL_STEP,
                           y + row * PIXEL_STEP);
            }
        }
    }
}

static void draw_colon(int32_t x, int32_t y)
{
    draw_pixel(x, y + 2 * PIXEL_STEP);
    draw_pixel(x, y + 4 * PIXEL_STEP);
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

    const int h = local_tm.tm_hour % 12;
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
    s_rect_used = 0;

    /* Always render two hour digits even in 12-hour mode. Keeping HH:MM at a
     * fixed width means 03:00 and 12:00 share the same visual center. */
    const int hour_w = 2 * DIGIT_W + DIGIT_GAP;
    const int minute_w = 2 * DIGIT_W + DIGIT_GAP;
    const int total_w = hour_w + COLON_GAP + COLON_W + COLON_GAP + minute_w;
    const int x0 = (UI_SCREEN_W - total_w) / 2;
    const int y0 = (UI_SCREEN_H - DIGIT_H) / 2;

    int x = x0;

    draw_digit((uint8_t)(hour12 / 10), x, y0);
    x += DIGIT_W + DIGIT_GAP;
    draw_digit((uint8_t)(hour12 % 10), x, y0);
    x += DIGIT_W + COLON_GAP;

    draw_colon(x, y0);
    x += COLON_W + COLON_GAP;

    draw_digit((uint8_t)(minute / 10), x, y0);
    x += DIGIT_W + DIGIT_GAP;
    draw_digit((uint8_t)(minute % 10), x, y0);

    draw_battery_status();
    finish_rect_frame();
}

static void refresh_power_state(void)
{
#if defined(ESP_PLATFORM)
    board_power_status_t power = {0};
    if (board_power_get_status(&power) != ESP_OK || !power.available) {
        return;
    }

    ui_clock_battery_state_t next = UI_CLOCK_BATTERY_NORMAL;
    if (power.charging) {
        next = UI_CLOCK_BATTERY_CHARGING;
    }
    else if (power.low_battery) {
        next = UI_CLOCK_BATTERY_LOW;
    }

    if (next != s_battery_state) {
        s_battery_state = next;
        s_last_hour = -1;
        s_last_minute = -1;
    }
#endif
}

static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_power_state();
    redraw_clock();
}

void ui_page_clock_set_battery_state(ui_clock_battery_state_t state)
{
    if (state != UI_CLOCK_BATTERY_NORMAL &&
        state != UI_CLOCK_BATTERY_LOW &&
        state != UI_CLOCK_BATTERY_CHARGING) {
        state = UI_CLOCK_BATTERY_NORMAL;
    }

    if (s_battery_state == state) {
        return;
    }

    s_battery_state = state;

    /* Force an immediate redraw even when the minute has not changed. */
    s_last_hour = -1;
    s_last_minute = -1;
    redraw_clock();
}

void ui_page_clock_build(lv_obj_t *parent)
{
    ui_page_clock_stop();

    s_parent = parent;
    lv_obj_null_on_delete(&s_parent);
    s_last_hour = -1;
    s_last_minute = -1;

    refresh_power_state();
    redraw_clock();
    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
}
