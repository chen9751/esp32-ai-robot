#include "ui_page_devices.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define W 640
#define H 172
#define PAGE_N 4
#define AIRCON_VIEW_N 2
#define CURTAIN_VIEW_N 1
#define BATH_VIEW_N 1
#define DRYING_VIEW_N 2

#define BG      lv_color_hex(0x000000)
#define BTN     lv_color_hex(0x111824)
#define BTN2    lv_color_hex(0x171F2D)
#define BR      lv_color_hex(0x526581)
#define BR_ON   lv_color_hex(0x7897C8)
#define PRESS   lv_color_hex(0x24344D)
#define CHECK   lv_color_hex(0x22324A)
#define CARD    lv_color_hex(0x0B121D)
#define CARD2   lv_color_hex(0x111D2D)
#define FG      lv_color_hex(0xEAF2FF)
#define DIM     lv_color_hex(0x536071)
#define DIS_BG  lv_color_hex(0x090D14)
#define DIS_BR  lv_color_hex(0x263142)
#define POWER   lv_color_hex(0xFF604F)

#define CURTAIN_SHADE_OUTER lv_color_hex(0x394553)
#define CURTAIN_SHADE_INNER lv_color_hex(0x667587)
#define CURTAIN_OPEN_EDGE_W 18
#define CURTAIN_CLOSED_GAP 12

#define DRYING_LINE_DIM    lv_color_hex(0x526071)
#define DRYING_LINE_BRIGHT lv_color_hex(0xAEBBCC)
#define DRYING_TOP_Y 24
#define DRYING_VERTICAL_Y 32
#define DRYING_BOTTOM_MIN_Y 42
#define DRYING_BOTTOM_MAX_Y 116

#define BATH_CURRENT_COLOR lv_color_hex(0xA4AFBD)
#define BATH_HIGH_BG       lv_color_hex(0x31547D)
#define BATH_HIGH_BG2      lv_color_hex(0x223D5E)
#define BATH_HIGH_BR       lv_color_hex(0xA5C4F3)
#define BATH_STOP_BG       lv_color_hex(0x361313)
#define BATH_STOP_BG2      lv_color_hex(0x651D19)
#define BATH_STOP_BR       lv_color_hex(0xB9463E)

#define TMIN 32
#define TMAX 62
#define TSTEP 22
#define FMIN 1
#define FMAX 7

#define BATH_TMIN_X10 160
#define BATH_TMAX_X10 310
#define BATH_TSTEP_X10 5
#define BATH_DRAG_STEP 22
#define GESTURE_LOCK_PX 6
#define QUICK_SWIPE_MS 220u
#define QUICK_SWIPE_PX 36

#define RI_POWER         "\xEF\x84\xA6" /* ri-shut-down-line U+F126 */
#define RI_COOL          "\xEF\x94\x92" /* ri-snowflake-line U+F512 */
#define RI_HEAT          "\xEF\x86\xBF" /* ri-sun-line U+F1BF */
#define RI_FAN           "\xEF\x8B\x8A" /* ri-windy-line U+F2CA */
#define RI_DRY           "\xEE\xB1\xAA" /* ri-drop-line U+EC6A */
#define RI_SWING         "\xEE\xA9\xA2" /* ri-arrow-left-right-line U+EA62 */
#define RI_THERM         "\xEF\x87\xB2" /* ri-temp-cold-line U+F1F2 */
#define RI_CURTAIN_OPEN  "\xEF\x8C\xA3" /* ri-expand-left-right-line U+F323 */
#define RI_CURTAIN_STOP  "\xEE\xBF\x98" /* ri-pause-line U+EFD8 */
#define RI_CURTAIN_CLOSE "\xEF\x8B\xBF" /* ri-contract-left-right-line U+F2FF */
#define RI_RACK_UP       "\xEE\xA9\xB6" /* ri-arrow-up-line U+EA76 */
#define RI_RACK_DOWN     "\xEE\xA9\x8C" /* ri-arrow-down-line U+EA4C */
#define RI_RACK_STOP     RI_CURTAIN_STOP
#define RI_BATH_VENT     "\xEF\x81\xA4" /* ri-refresh-line U+F064 */
#define RI_BATH_BLOW     RI_FAN
#define RI_BATH_WARM     RI_HEAT
#define RI_BATH_DRY      RI_DRY
#define RI_BATH_STOP     "\xEF\x86\x9F" /* ri-stop-circle-line U+F19F */

#if defined(UI_DEVICES_HAS_FONTS)
LV_FONT_DECLARE(ui_font_source_han_devices_16);
LV_FONT_DECLARE(ui_font_remix_devices_28);
LV_FONT_DECLARE(ui_font_remix_devices_56);
#define TF  (&ui_font_source_han_devices_16)
#define IF  (&ui_font_remix_devices_28)
#define ILF (&ui_font_remix_devices_56)
#else
#define TF  LV_FONT_DEFAULT
#define IF  LV_FONT_DEFAULT
#define ILF LV_FONT_DEFAULT
#endif

typedef struct { lv_obj_t *seg[7]; } digit_t;
typedef enum { CENTER_TEMP, CENTER_MODE, CENTER_FAN } center_t;
typedef enum { GESTURE_PENDING, GESTURE_VERTICAL, GESTURE_HORIZONTAL } gesture_axis_t;

typedef struct {
    lv_obj_t *power, *power_i, *mode, *mode_i, *swing, *swing_i, *fan, *fan_i;
    lv_obj_t *card, *temp, *dot, *off_i, *temp_g;
    digit_t d[3];
    lv_obj_t *mode_p, *mode_b[4], *fan_p, *fan_v, *slider, *auto_b, *feat[4];
} aircon_view_t;

typedef struct {
    lv_obj_t *left_shade;
    lv_obj_t *right_shade;
    lv_obj_t *button[3];
} curtain_view_t;

typedef struct {
    lv_obj_t *temp;
    digit_t d[3];
    lv_obj_t *dot;
    lv_obj_t *temp_gesture;
    lv_obj_t *function_btn[4];
    lv_obj_t *stop_btn;
    lv_obj_t *stop_icon;
} bath_view_t;

typedef struct {
    lv_obj_t *top_bar;
    lv_obj_t *left_line;
    lv_obj_t *right_line;
    lv_obj_t *bottom_bar;
    lv_obj_t *up_button;
    lv_obj_t *down_button;
    lv_obj_t *stop_button;
} drying_view_t;

enum { CURTAIN_ACTION_OPEN = 0, CURTAIN_ACTION_STOP, CURTAIN_ACTION_CLOSE };
enum { DRYING_ACTION_UP = 0, DRYING_ACTION_DOWN, DRYING_ACTION_STOP };
enum { BATH_VENT = 0, BATH_BLOW, BATH_WARM, BATH_DRY };

static const char *feat_names[4] = { "睡眠", "ECO", "干燥", "辅热" };
static const char *mode_icons[4] = { RI_COOL, RI_HEAT, RI_FAN, RI_DRY };
static const char *bath_icons[4] = { RI_BATH_VENT, RI_BATH_BLOW, RI_BATH_WARM, RI_BATH_DRY };
static const uint8_t masks[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static lv_obj_t *root;
static lv_obj_t *pager;
static bool wrapping;
static bool updating;
static ui_devices_activity_cb_t acb;
static void *aud;

static aircon_view_t aircon_views[AIRCON_VIEW_N];
static size_t aircon_view_n;
static bool power_on = true;
static bool swing_on;
static bool features[4];
static uint8_t mode_idx;
static int temp2 = 48;
static int fan_speed = 4;
static bool fan_auto;
static center_t center = CENTER_TEMP;
static int32_t temp_x;
static int32_t temp_y;
static int32_t temp_press_y0;
static int temp_start2;
static uint32_t temp_press_tick;
static gesture_axis_t temp_axis = GESTURE_PENDING;

static curtain_view_t curtain_views[CURTAIN_VIEW_N];
static size_t curtain_view_n;
/* 0 = fully closed, 100 = fully open. */
static int32_t curtain_open_pct = 58;

static bath_view_t bath_views[BATH_VIEW_N];
static size_t bath_view_n;
static uint8_t bath_speed[3]; /* Ventilation, blower and warm air: 0 off, 1 low, 2 high. */
static bool bath_dry_on;
static int32_t bath_target_x10 = 260;
static int32_t bath_current_x10 = 240;
static int32_t bath_temp_x;
static int32_t bath_temp_y;
static int32_t bath_press_y0;
static int32_t bath_start_x10;
static uint32_t bath_press_tick;
static gesture_axis_t bath_temp_axis = GESTURE_PENDING;

static drying_view_t drying_views[DRYING_VIEW_N];
static size_t drying_view_n;
/* 0 = top, 100 = lowest visual position. */
static int32_t drying_position_pct = 56;

static void activity(void)
{
    if(acb) acb(aud);
}

static int32_t iabs32(int32_t v)
{
    return v < 0 ? -v : v;
}

static void hidden(lv_obj_t *o, bool h)
{
    if(!o) return;
    if(h) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void enabled(lv_obj_t *o, bool e)
{
    if(!o) return;
    if(e) lv_obj_remove_state(o, LV_STATE_DISABLED);
    else lv_obj_add_state(o, LV_STATE_DISABLED);
}

static void checked(lv_obj_t *o, bool c)
{
    if(!o) return;
    if(c) lv_obj_add_state(o, LV_STATE_CHECKED);
    else lv_obj_remove_state(o, LV_STATE_CHECKED);
}

static void style_btn(lv_obj_t *o, int r)
{
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_bg_color(o, BTN, 0);
    lv_obj_set_style_bg_grad_color(o, BTN2, 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, BR, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(o, PRESS, LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(o, PRESS, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(o, BR_ON, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(o, CHECK, LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_color(o, PRESS, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(o, BR_ON, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(o, DIS_BG, LV_STATE_DISABLED);
    lv_obj_set_style_bg_grad_color(o, DIS_BG, LV_STATE_DISABLED);
    lv_obj_set_style_border_color(o, DIS_BR, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(o, DIM, LV_STATE_DISABLED);
}

static lv_obj_t *icon_button(lv_obj_t *p, const char *g, int x, int y,
                             int w, int h, bool checkable,
                             const lv_font_t *font, lv_obj_t **icon_out)
{
    lv_obj_t *b = lv_obj_create(p);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    style_btn(b, 18);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    if(checkable) lv_obj_add_flag(b, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_t *i = lv_label_create(b);
    lv_label_set_text(i, g);
    lv_obj_set_style_text_font(i, font, 0);
    lv_obj_set_style_text_color(i, FG, 0);
    lv_obj_center(i);
    lv_obj_clear_flag(i, LV_OBJ_FLAG_CLICKABLE);
    if(icon_out) *icon_out = i;
    return b;
}

static lv_obj_t *ibtn(lv_obj_t *p, const char *g, int x, int y,
                      int w, int h, bool ck, lv_obj_t **io)
{
    return icon_button(p, g, x, y, w, h, ck, IF, io);
}

static lv_obj_t *tbtn(lv_obj_t *p, const char *t, int x, int y, int w, int h)
{
    lv_obj_t *b = lv_obj_create(p);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    style_btn(b, 18);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, TF, 0);
    lv_obj_set_style_text_color(l, FG, 0);
    lv_obj_center(l);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return b;
}

static lv_obj_t *plain_bar(lv_obj_t *p, int x, int y, int w, int h, lv_color_t color)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, h / 2, 0);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *page(lv_obj_t *p)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, W, H);
    lv_obj_set_style_bg_color(o, BG, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* ---------- Shared seven-segment digits ---------- */

static lv_obj_t *seg(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 3, 0);
    lv_obj_set_style_bg_color(o, FG, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void build_digit(lv_obj_t *p, digit_t *d, int x)
{
    lv_obj_t *h = lv_obj_create(p);
    lv_obj_remove_style_all(h);
    lv_obj_set_pos(h, x, 0);
    lv_obj_set_size(h, 42, 98);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    d->seg[0] = seg(h, 9, 2, 24, 6);
    d->seg[1] = seg(h, 34, 10, 6, 35);
    d->seg[2] = seg(h, 34, 53, 6, 35);
    d->seg[3] = seg(h, 9, 90, 24, 6);
    d->seg[4] = seg(h, 2, 53, 6, 35);
    d->seg[5] = seg(h, 2, 10, 6, 35);
    d->seg[6] = seg(h, 9, 46, 24, 6);
}

static void set_digit(digit_t *d, uint8_t n, lv_color_t c)
{
    uint8_t m = n < 10 ? masks[n] : 0;
    for(int i = 0; i < 7; ++i) {
        lv_obj_set_style_bg_color(d->seg[i], c, 0);
        lv_obj_set_style_bg_opa(d->seg[i],
                                (m & (1u << i)) ? LV_OPA_COVER : LV_OPA_TRANSP,
                                0);
    }
}

static void set_three_digit_temp(digit_t d[3], lv_obj_t *dot, int32_t temp_x10, lv_color_t c)
{
    if(temp_x10 < 0) temp_x10 = 0;
    if(temp_x10 > 999) temp_x10 = 999;
    int32_t whole = temp_x10 / 10;
    set_digit(&d[0], (uint8_t)((whole / 10) % 10), c);
    set_digit(&d[1], (uint8_t)(whole % 10), c);
    set_digit(&d[2], (uint8_t)(temp_x10 % 10), c);
    lv_obj_set_style_bg_color(dot, c, 0);
}

/* ---------- Air conditioner ---------- */

static void temp_refresh(aircon_view_t *v)
{
    int whole = temp2 / 2;
    lv_color_t c = power_on ? FG : DIM;
    set_digit(&v->d[0], (uint8_t)(whole / 10), c);
    set_digit(&v->d[1], (uint8_t)(whole % 10), c);
    set_digit(&v->d[2], (uint8_t)((temp2 & 1) ? 5 : 0), c);
    lv_obj_set_style_bg_color(v->dot, c, 0);
}

static void refresh_aircon(void)
{
    char ft[8];
    snprintf(ft, sizeof(ft), "%d", fan_speed);
    updating = true;
    for(size_t i = 0; i < aircon_view_n; ++i) {
        aircon_view_t *v = &aircon_views[i];
        bool sm = power_on && center == CENTER_MODE;
        bool sf = power_on && center == CENTER_FAN;
        bool st = !sm && !sf;

        checked(v->power, power_on);
        checked(v->mode, sm);
        checked(v->swing, swing_on);
        checked(v->fan, sf);
        checked(v->auto_b, fan_auto);
        for(int j = 0; j < 4; ++j) {
            checked(v->feat[j], features[j]);
            checked(v->mode_b[j], mode_idx == (uint8_t)j);
            enabled(v->feat[j], power_on);
            enabled(v->mode_b[j], sm);
            lv_obj_t *feat_label = lv_obj_get_child(v->feat[j], 0);
            if(feat_label) lv_obj_set_style_text_color(feat_label, power_on ? FG : DIM, 0);
        }
        enabled(v->mode, power_on);
        enabled(v->swing, power_on);
        enabled(v->fan, power_on);
        enabled(v->temp_g, power_on && st);
        enabled(v->slider, sf);
        enabled(v->auto_b, sf);
        lv_label_set_text(v->mode_i, mode_icons[mode_idx]);
        lv_slider_set_value(v->slider, fan_speed, LV_ANIM_OFF);
        lv_obj_set_style_text_color(v->power_i, POWER, 0);
        lv_obj_set_style_text_color(v->mode_i, power_on ? FG : DIM, 0);
        lv_obj_set_style_text_color(v->swing_i, power_on ? FG : DIM, 0);
        lv_obj_set_style_text_color(v->fan_i, power_on ? FG : DIM, 0);
        temp_refresh(v);
        lv_label_set_text(v->fan_v, fan_auto ? "AUTO" : ft);
        hidden(v->temp, !st || !power_on);
        hidden(v->off_i, !st || power_on);
        hidden(v->temp_g, !st || !power_on);
        hidden(v->card, st);
        hidden(v->mode_p, !sm);
        hidden(v->fan_p, !sf);
    }
    updating = false;
}

static void power_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    power_on = !power_on;
    if(!power_on) center = CENTER_TEMP;
    refresh_aircon(); activity();
}

static void mode_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED || !power_on) return;
    center = center == CENTER_MODE ? CENTER_TEMP : CENTER_MODE;
    refresh_aircon(); activity();
}

static void swing_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED || !power_on) return;
    swing_on = !swing_on; refresh_aircon(); activity();
}

static void fan_btn_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED || !power_on) return;
    center = center == CENTER_FAN ? CENTER_TEMP : CENTER_FAN;
    refresh_aircon(); activity();
}

static void feat_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED || !power_on) return;
    intptr_t n = (intptr_t)lv_event_get_user_data(e);
    if(n < 0 || n > 3) return;
    features[n] = !features[n]; refresh_aircon(); activity();
}

static void mode_opt_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED || !power_on) return;
    intptr_t n = (intptr_t)lv_event_get_user_data(e);
    if(n < 0 || n > 3) return;
    mode_idx = (uint8_t)n; center = CENTER_TEMP;
    refresh_aircon(); activity();
}

static void temp_cb(lv_event_t *e)
{
    if(!power_on || center != CENTER_TEMP) return;
    lv_indev_t *i = lv_event_get_indev(e);
    if(!i) return;
    lv_event_code_t c = lv_event_get_code(e);
    lv_point_t p; lv_indev_get_point(i, &p);

    if(c == LV_EVENT_PRESSED) {
        temp_x = p.x;
        temp_y = p.y;
        temp_press_y0 = p.y;
        temp_start2 = temp2;
        temp_press_tick = lv_tick_get();
        temp_axis = GESTURE_PENDING;
        activity();
        return;
    }

    if(c == LV_EVENT_PRESSING) {
        const int32_t dx = p.x - temp_x;
        const int32_t total_dy = p.y - temp_press_y0;

        if(temp_axis == GESTURE_PENDING) {
            const int32_t ax = iabs32(dx);
            const int32_t ay = iabs32(total_dy);
            if(ax < GESTURE_LOCK_PX && ay < GESTURE_LOCK_PX) {
                activity();
                return;
            }
            if(ax > ay) {
                temp_axis = GESTURE_HORIZONTAL;
                activity();
                return;
            }
            temp_axis = GESTURE_VERTICAL;
        }

        if(temp_axis == GESTURE_HORIZONTAL) {
            activity();
            return;
        }

        /* Track from the original press point instead of ratcheting/rebasing
         * after every half-degree. This keeps the displayed value coupled to
         * finger travel and avoids the "sticky then suddenly jumps" feel. */
        int steps = 0;
        if(total_dy <= -TSTEP) steps = (-total_dy) / TSTEP;
        else if(total_dy >= TSTEP) steps = -(total_dy / TSTEP);

        int target = temp_start2 + steps;
        if(target < TMIN) target = TMIN;
        if(target > TMAX) target = TMAX;
        if(target != temp2) {
            temp2 = target;
            refresh_aircon();
        }
        activity();
        return;
    }

    if(c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
        if(c == LV_EVENT_RELEASED && temp_axis != GESTURE_HORIZONTAL) {
            const int32_t total_dy = p.y - temp_press_y0;
            const uint32_t elapsed = lv_tick_get() - temp_press_tick;
            if(elapsed <= QUICK_SWIPE_MS && iabs32(total_dy) >= QUICK_SWIPE_PX) {
                /* A deliberate flick is one whole degree regardless of travel.
                 * temp2 uses half-degree units, so +/-2 == +/-1.0 C. */
                int target = temp_start2 + (total_dy < 0 ? 2 : -2);
                if(target < TMIN) target = TMIN;
                if(target > TMAX) target = TMAX;
                temp2 = target;
                refresh_aircon();
            }
        }
        temp_axis = GESTURE_PENDING;
        activity();
    }
}

static void auto_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_LONG_PRESSED ||
       !power_on || center != CENTER_FAN) return;
    fan_auto = true; refresh_aircon(); activity();
}

static void slider_cb(lv_event_t *e)
{
    if(!power_on || updating || center != CENTER_FAN) return;
    lv_event_code_t c = lv_event_get_code(e);
    if(c == LV_EVENT_PRESSED || c == LV_EVENT_PRESSING || c == LV_EVENT_VALUE_CHANGED) {
        fan_speed = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
        fan_auto = false; refresh_aircon();
    }
    if(c == LV_EVENT_PRESSED || c == LV_EVENT_PRESSING || c == LV_EVENT_VALUE_CHANGED ||
       c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) activity();
}

static void card_style(lv_obj_t *o)
{
    lv_obj_set_style_radius(o, 24, 0);
    lv_obj_set_style_bg_color(o, CARD, 0);
    lv_obj_set_style_bg_grad_color(o, CARD2, 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, BR, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_60, 0);
}

static void build_aircon(lv_obj_t *p)
{
    if(aircon_view_n >= AIRCON_VIEW_N) return;
    aircon_view_t *v = &aircon_views[aircon_view_n++];
    lv_obj_t *pg = page(p);
    const int lx[2] = { 68, 148 }, rx[2] = { 420, 500 }, yy[2] = { 33, 97 };
    const int bw = 72, bh = 42;

    v->power = ibtn(pg, RI_POWER, lx[0], yy[0], bw, bh, false, &v->power_i);
    lv_obj_set_style_text_color(v->power_i, POWER, 0);
    lv_obj_add_event_cb(v->power, power_cb, LV_EVENT_CLICKED, NULL);
    v->mode = ibtn(pg, mode_icons[mode_idx], lx[1], yy[0], bw, bh, false, &v->mode_i);
    lv_obj_add_event_cb(v->mode, mode_cb, LV_EVENT_CLICKED, NULL);
    v->swing = ibtn(pg, RI_SWING, lx[0], yy[1], bw, bh, true, &v->swing_i);
    lv_obj_add_event_cb(v->swing, swing_cb, LV_EVENT_CLICKED, NULL);
    v->fan = ibtn(pg, RI_FAN, lx[1], yy[1], bw, bh, false, &v->fan_i);
    lv_obj_add_event_cb(v->fan, fan_btn_cb, LV_EVENT_CLICKED, NULL);

    v->temp = lv_obj_create(pg); lv_obj_remove_style_all(v->temp);
    lv_obj_set_pos(v->temp, 248, 37); lv_obj_set_size(v->temp, 145, 100);
    lv_obj_clear_flag(v->temp, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    build_digit(v->temp, &v->d[0], 0); build_digit(v->temp, &v->d[1], 45); build_digit(v->temp, &v->d[2], 103);
    v->dot = plain_bar(v->temp, 92, 86, 8, 8, FG);
    lv_obj_set_style_radius(v->dot, LV_RADIUS_CIRCLE, 0);

    v->off_i = lv_label_create(pg); lv_label_set_text(v->off_i, RI_THERM);
    lv_obj_set_style_text_font(v->off_i, ILF, 0); lv_obj_set_style_text_color(v->off_i, DIM, 0);
    lv_obj_set_width(v->off_i, 184); lv_obj_set_style_text_align(v->off_i, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(v->off_i, 228, 58);

    v->temp_g = lv_obj_create(pg); lv_obj_remove_style_all(v->temp_g);
    lv_obj_set_pos(v->temp_g, 228, 20); lv_obj_set_size(v->temp_g, 184, 134);
    lv_obj_set_style_bg_opa(v->temp_g, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(v->temp_g, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(v->temp_g, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(v->temp_g, temp_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(v->temp_g, temp_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(v->temp_g, temp_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(v->temp_g, temp_cb, LV_EVENT_PRESS_LOST, NULL);

    v->card = lv_obj_create(pg); lv_obj_remove_style_all(v->card);
    lv_obj_set_pos(v->card, 228, 25); lv_obj_set_size(v->card, 184, 122); card_style(v->card);
    lv_obj_clear_flag(v->card, LV_OBJ_FLAG_SCROLLABLE);

    v->mode_p = lv_obj_create(v->card); lv_obj_remove_style_all(v->mode_p);
    lv_obj_set_pos(v->mode_p, 9, 12); lv_obj_set_size(v->mode_p, 166, 98);
    lv_obj_clear_flag(v->mode_p, LV_OBJ_FLAG_SCROLLABLE);
    for(int i = 0; i < 4; ++i) {
        int c = i & 1, r = i >> 1;
        v->mode_b[i] = ibtn(v->mode_p, mode_icons[i], c * 88, r * 56, 78, 42, false, NULL);
        lv_obj_set_style_radius(v->mode_b[i], 14, 0);
        lv_obj_add_event_cb(v->mode_b[i], mode_opt_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    v->fan_p = lv_obj_create(v->card); lv_obj_remove_style_all(v->fan_p);
    lv_obj_set_pos(v->fan_p, 8, 11); lv_obj_set_size(v->fan_p, 168, 100);
    lv_obj_clear_flag(v->fan_p, LV_OBJ_FLAG_SCROLLABLE);
    v->fan_v = lv_label_create(v->fan_p); lv_obj_set_width(v->fan_v, 168);
    lv_obj_set_style_text_align(v->fan_v, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(v->fan_v, TF, 0); lv_obj_set_style_text_color(v->fan_v, FG, 0);
    v->slider = lv_slider_create(v->fan_p); lv_obj_set_pos(v->slider, 10, 34); lv_obj_set_size(v->slider, 148, 8);
    lv_slider_set_range(v->slider, FMIN, FMAX);
    lv_obj_set_style_radius(v->slider, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(v->slider, BR, LV_PART_MAIN); lv_obj_set_style_bg_opa(v->slider, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_bg_color(v->slider, FG, LV_PART_INDICATOR); lv_obj_set_style_bg_color(v->slider, FG, LV_PART_KNOB);
    lv_obj_set_style_pad_all(v->slider, 4, LV_PART_KNOB); lv_obj_add_event_cb(v->slider, slider_cb, LV_EVENT_ALL, NULL);
    v->auto_b = tbtn(v->fan_p, "AUTO", 44, 60, 80, 28);
    lv_obj_clear_flag(v->auto_b, LV_OBJ_FLAG_CHECKABLE); lv_obj_add_event_cb(v->auto_b, auto_cb, LV_EVENT_LONG_PRESSED, NULL);

    for(int i = 0; i < 4; ++i) {
        int c = i & 1, r = i >> 1;
        v->feat[i] = tbtn(pg, feat_names[i], rx[c], yy[r], bw, bh);
        lv_obj_add_event_cb(v->feat[i], feat_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    refresh_aircon();
}

/* ---------- Curtain ---------- */

static int32_t clamp_pct(int32_t percent)
{
    if(percent < 0) return 0;
    if(percent > 100) return 100;
    return percent;
}

static void curtain_apply_position(void)
{
    const int32_t max_panel_w = (W - CURTAIN_CLOSED_GAP) / 2;
    const int32_t travel = max_panel_w - CURTAIN_OPEN_EDGE_W;
    int32_t closed = 100 - curtain_open_pct;
    int32_t panel_w = CURTAIN_OPEN_EDGE_W + (travel * closed) / 100;
    for(size_t i = 0; i < curtain_view_n; ++i) {
        curtain_view_t *v = &curtain_views[i];
        lv_obj_set_width(v->left_shade, panel_w);
        lv_obj_set_width(v->right_shade, panel_w);
        lv_obj_set_x(v->right_shade, W - panel_w);
    }
}

static void curtain_anim_exec(void *var, int32_t value)
{
    *(int32_t *)var = clamp_pct(value); curtain_apply_position();
}

static void curtain_stop_animation(void)
{
    lv_anim_delete(&curtain_open_pct, curtain_anim_exec);
}

static void curtain_start_animation(int32_t target)
{
    target = clamp_pct(target); curtain_stop_animation();
    if(target == curtain_open_pct) return;
    int32_t d = target > curtain_open_pct ? target - curtain_open_pct : curtain_open_pct - target;
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, &curtain_open_pct);
    lv_anim_set_exec_cb(&a, curtain_anim_exec); lv_anim_set_values(&a, curtain_open_pct, target);
    lv_anim_set_duration(&a, 500u + (uint32_t)d * 45u);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); lv_anim_start(&a);
}

static void curtain_action_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    intptr_t a = (intptr_t)lv_event_get_user_data(e);
    if(a == CURTAIN_ACTION_OPEN) curtain_start_animation(100);
    else if(a == CURTAIN_ACTION_STOP) curtain_stop_animation();
    else if(a == CURTAIN_ACTION_CLOSE) curtain_start_animation(0);
    activity();
}

static lv_obj_t *curtain_button(lv_obj_t *parent, const char *icon, int32_t x, intptr_t action)
{
    lv_obj_t *b = icon_button(parent, icon, x, 36, 132, 100, false, ILF, NULL);
    lv_obj_set_style_radius(b, 24, 0);
    lv_obj_add_event_cb(b, curtain_action_cb, LV_EVENT_CLICKED, (void *)action);
    return b;
}

static void build_curtain(lv_obj_t *p)
{
    if(curtain_view_n >= CURTAIN_VIEW_N) return;
    curtain_view_t *v = &curtain_views[curtain_view_n++];
    lv_obj_t *pg = page(p);

    v->left_shade = lv_obj_create(pg); lv_obj_remove_style_all(v->left_shade);
    lv_obj_set_pos(v->left_shade, 0, 0); lv_obj_set_height(v->left_shade, H);
    lv_obj_set_style_bg_color(v->left_shade, CURTAIN_SHADE_OUTER, 0);
    lv_obj_set_style_bg_grad_color(v->left_shade, CURTAIN_SHADE_INNER, 0);
    lv_obj_set_style_bg_grad_dir(v->left_shade, LV_GRAD_DIR_HOR, 0); lv_obj_set_style_bg_opa(v->left_shade, LV_OPA_40, 0);
    lv_obj_clear_flag(v->left_shade, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    v->right_shade = lv_obj_create(pg); lv_obj_remove_style_all(v->right_shade);
    lv_obj_set_pos(v->right_shade, W, 0); lv_obj_set_height(v->right_shade, H);
    lv_obj_set_style_bg_color(v->right_shade, CURTAIN_SHADE_INNER, 0);
    lv_obj_set_style_bg_grad_color(v->right_shade, CURTAIN_SHADE_OUTER, 0);
    lv_obj_set_style_bg_grad_dir(v->right_shade, LV_GRAD_DIR_HOR, 0); lv_obj_set_style_bg_opa(v->right_shade, LV_OPA_40, 0);
    lv_obj_clear_flag(v->right_shade, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    v->button[0] = curtain_button(pg, RI_CURTAIN_OPEN, 82, CURTAIN_ACTION_OPEN);
    v->button[1] = curtain_button(pg, RI_CURTAIN_STOP, 254, CURTAIN_ACTION_STOP);
    v->button[2] = curtain_button(pg, RI_CURTAIN_CLOSE, 426, CURTAIN_ACTION_CLOSE);
    curtain_apply_position();
}

void ui_page_devices_set_curtain_position(uint8_t percent)
{
    curtain_stop_animation(); curtain_open_pct = clamp_pct(percent); curtain_apply_position();
}

/* ---------- Bath heater ---------- */

static bool bath_active(void)
{
    return bath_speed[0] || bath_speed[1] || bath_speed[2] || bath_dry_on;
}

static void bath_style_level(lv_obj_t *button, uint8_t level)
{
    lv_obj_set_style_bg_color(button, BTN, 0);
    lv_obj_set_style_bg_grad_color(button, BTN2, 0);
    lv_obj_set_style_border_color(button, BR, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_opa(button, LV_OPA_70, 0);
    if(level == 1) {
        lv_obj_set_style_bg_color(button, CHECK, 0);
        lv_obj_set_style_bg_grad_color(button, PRESS, 0);
        lv_obj_set_style_border_color(button, BR_ON, 0);
        lv_obj_set_style_border_opa(button, LV_OPA_COVER, 0);
    } else if(level >= 2) {
        lv_obj_set_style_bg_color(button, BATH_HIGH_BG, 0);
        lv_obj_set_style_bg_grad_color(button, BATH_HIGH_BG2, 0);
        lv_obj_set_style_border_color(button, BATH_HIGH_BR, 0);
        lv_obj_set_style_border_width(button, 2, 0);
        lv_obj_set_style_border_opa(button, LV_OPA_COVER, 0);
    }
}

static void refresh_bath(void)
{
    bool active = bath_active();
    int32_t display_x10 = active ? bath_target_x10 : bath_current_x10;
    lv_color_t color = active ? FG : BATH_CURRENT_COLOR;

    for(size_t i = 0; i < bath_view_n; ++i) {
        bath_view_t *v = &bath_views[i];
        set_three_digit_temp(v->d, v->dot, display_x10, color);
        enabled(v->temp_gesture, active);
        bath_style_level(v->function_btn[BATH_VENT], bath_speed[0]);
        bath_style_level(v->function_btn[BATH_BLOW], bath_speed[1]);
        bath_style_level(v->function_btn[BATH_WARM], bath_speed[2]);
        bath_style_level(v->function_btn[BATH_DRY], bath_dry_on ? 1 : 0);
    }
}

static void bath_clear_modes(void)
{
    bath_speed[0] = bath_speed[1] = bath_speed[2] = 0;
    bath_dry_on = false;
}

static void bath_function_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    intptr_t fn = (intptr_t)lv_event_get_user_data(e);
    if(fn < 0 || fn > 3) return;

    if(fn == BATH_DRY) {
        bool was_dry = bath_dry_on;
        bath_clear_modes();
        bath_dry_on = !was_dry;
    } else {
        uint8_t old_level = bath_speed[fn];
        bath_clear_modes();
        bath_speed[fn] = old_level == 0 ? 1 : (old_level == 1 ? 2 : 1);
    }

    refresh_bath();
    activity();
}

static void bath_stop_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    bath_clear_modes();
    refresh_bath();
    activity();
}

static void bath_temp_cb(lv_event_t *e)
{
    if(!bath_active()) return;
    lv_indev_t *indev = lv_event_get_indev(e);
    if(!indev) return;
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if(code == LV_EVENT_PRESSED) {
        bath_temp_x = p.x;
        bath_temp_y = p.y;
        bath_press_y0 = p.y;
        bath_start_x10 = bath_target_x10;
        bath_press_tick = lv_tick_get();
        bath_temp_axis = GESTURE_PENDING;
        activity();
        return;
    }

    if(code == LV_EVENT_PRESSING) {
        const int32_t dx = p.x - bath_temp_x;
        const int32_t total_dy = p.y - bath_press_y0;

        if(bath_temp_axis == GESTURE_PENDING) {
            const int32_t ax = iabs32(dx);
            const int32_t ay = iabs32(total_dy);
            if(ax < GESTURE_LOCK_PX && ay < GESTURE_LOCK_PX) {
                activity();
                return;
            }
            if(ax > ay) {
                bath_temp_axis = GESTURE_HORIZONTAL;
                activity();
                return;
            }
            bath_temp_axis = GESTURE_VERTICAL;
        }

        if(bath_temp_axis == GESTURE_HORIZONTAL) {
            activity();
            return;
        }

        int steps = 0;
        if(total_dy <= -BATH_DRAG_STEP) steps = (-total_dy) / BATH_DRAG_STEP;
        else if(total_dy >= BATH_DRAG_STEP) steps = -(total_dy / BATH_DRAG_STEP);

        int32_t target = bath_start_x10 + steps * BATH_TSTEP_X10;
        if(target < BATH_TMIN_X10) target = BATH_TMIN_X10;
        if(target > BATH_TMAX_X10) target = BATH_TMAX_X10;
        if(target != bath_target_x10) {
            bath_target_x10 = target;
            refresh_bath();
        }
        activity();
        return;
    }

    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if(code == LV_EVENT_RELEASED && bath_temp_axis != GESTURE_HORIZONTAL) {
            const int32_t total_dy = p.y - bath_press_y0;
            const uint32_t elapsed = lv_tick_get() - bath_press_tick;
            if(elapsed <= QUICK_SWIPE_MS && iabs32(total_dy) >= QUICK_SWIPE_PX) {
                /* Fast flick = exactly 1 C. Slow drag remains 0.5 C per step. */
                int32_t target = bath_start_x10 + (total_dy < 0 ? 10 : -10);
                if(target < BATH_TMIN_X10) target = BATH_TMIN_X10;
                if(target > BATH_TMAX_X10) target = BATH_TMAX_X10;
                bath_target_x10 = target;
                refresh_bath();
            }
        }
        bath_temp_axis = GESTURE_PENDING;
        activity();
    }
}

static void build_bath(lv_obj_t *p)
{
    if(bath_view_n >= BATH_VIEW_N) return;
    bath_view_t *v = &bath_views[bath_view_n++];
    lv_obj_t *pg = page(p);

    v->temp = lv_obj_create(pg);
    lv_obj_remove_style_all(v->temp);
    lv_obj_set_pos(v->temp, 92, 37);
    lv_obj_set_size(v->temp, 145, 100);
    lv_obj_clear_flag(v->temp, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    build_digit(v->temp, &v->d[0], 0);
    build_digit(v->temp, &v->d[1], 45);
    build_digit(v->temp, &v->d[2], 103);
    v->dot = plain_bar(v->temp, 92, 86, 8, 8, FG);
    lv_obj_set_style_radius(v->dot, LV_RADIUS_CIRCLE, 0);

    v->temp_gesture = lv_obj_create(pg);
    lv_obj_remove_style_all(v->temp_gesture);
    lv_obj_set_pos(v->temp_gesture, 68, 20);
    lv_obj_set_size(v->temp_gesture, 194, 134);
    lv_obj_set_style_bg_opa(v->temp_gesture, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(v->temp_gesture, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(v->temp_gesture, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(v->temp_gesture, bath_temp_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(v->temp_gesture, bath_temp_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(v->temp_gesture, bath_temp_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(v->temp_gesture, bath_temp_cb, LV_EVENT_PRESS_LOST, NULL);

    const int bx[2] = { 336, 426 };
    const int by[2] = { 34, 92 };
    for(int i = 0; i < 4; ++i) {
        int col = i & 1;
        int row = i >> 1;
        v->function_btn[i] = ibtn(pg, bath_icons[i], bx[col], by[row], 80, 48, false, NULL);
        lv_obj_set_style_radius(v->function_btn[i], 16, 0);
        lv_obj_add_event_cb(v->function_btn[i], bath_function_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    v->stop_btn = ibtn(pg, RI_BATH_STOP, 526, 34, 82, 106, false, &v->stop_icon);
    lv_obj_set_style_radius(v->stop_btn, 20, 0);
    lv_obj_set_style_bg_color(v->stop_btn, BATH_STOP_BG, 0);
    lv_obj_set_style_bg_grad_color(v->stop_btn, BATH_STOP_BG2, 0);
    lv_obj_set_style_border_color(v->stop_btn, BATH_STOP_BR, 0);
    lv_obj_set_style_border_opa(v->stop_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(v->stop_icon, POWER, 0);
    lv_obj_set_style_bg_color(v->stop_btn, lv_color_hex(0x74231D), LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(v->stop_btn, lv_color_hex(0x8A2C24), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(v->stop_btn, lv_color_hex(0xE1645A), LV_STATE_PRESSED);
    lv_obj_add_event_cb(v->stop_btn, bath_stop_cb, LV_EVENT_CLICKED, NULL);

    refresh_bath();
}

void ui_page_devices_set_bath_current_temperature(int16_t temperature_x10)
{
    if(temperature_x10 < 0) temperature_x10 = 0;
    if(temperature_x10 > 999) temperature_x10 = 999;
    bath_current_x10 = temperature_x10;
    if(!bath_active()) refresh_bath();
}

/* ---------- Drying rack ---------- */

static void drying_apply_position(void)
{
    int32_t travel = DRYING_BOTTOM_MAX_Y - DRYING_BOTTOM_MIN_Y;
    int32_t bottom_y = DRYING_BOTTOM_MIN_Y + (travel * drying_position_pct) / 100;
    int32_t line_h = bottom_y - DRYING_VERTICAL_Y + 2;
    if(line_h < 6) line_h = 6;
    for(size_t i = 0; i < drying_view_n; ++i) {
        drying_view_t *v = &drying_views[i];
        lv_obj_set_height(v->left_line, line_h);
        lv_obj_set_height(v->right_line, line_h);
        lv_obj_set_y(v->bottom_bar, bottom_y);
    }
}

static void drying_anim_exec(void *var, int32_t value)
{
    *(int32_t *)var = clamp_pct(value); drying_apply_position();
}

static void drying_stop_animation(void)
{
    lv_anim_delete(&drying_position_pct, drying_anim_exec);
}

static void drying_start_animation(int32_t target)
{
    target = clamp_pct(target); drying_stop_animation();
    if(target == drying_position_pct) return;
    int32_t d = target > drying_position_pct ? target - drying_position_pct : drying_position_pct - target;
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, &drying_position_pct);
    lv_anim_set_exec_cb(&a, drying_anim_exec); lv_anim_set_values(&a, drying_position_pct, target);
    lv_anim_set_duration(&a, 450u + (uint32_t)d * 34u);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); lv_anim_start(&a);
}

static void drying_action_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    intptr_t action = (intptr_t)lv_event_get_user_data(e);
    if(action == DRYING_ACTION_UP) drying_start_animation(0);
    else if(action == DRYING_ACTION_DOWN) drying_start_animation(100);
    else if(action == DRYING_ACTION_STOP) drying_stop_animation();
    activity();
}

static lv_obj_t *drying_button(lv_obj_t *parent, const char *icon,
                               int x, int y, int w, int h, intptr_t action)
{
    lv_obj_t *b = icon_button(parent, icon, x, y, w, h, false, ILF, NULL);
    lv_obj_set_style_radius(b, 22, 0);
    lv_obj_add_event_cb(b, drying_action_cb, LV_EVENT_CLICKED, (void *)action);
    return b;
}

static void build_drying(lv_obj_t *p)
{
    if(drying_view_n >= DRYING_VIEW_N) return;
    drying_view_t *v = &drying_views[drying_view_n++];
    lv_obj_t *pg = page(p);
    v->up_button = drying_button(pg, RI_RACK_UP, 76, 24, 96, 56, DRYING_ACTION_UP);
    v->down_button = drying_button(pg, RI_RACK_DOWN, 76, 92, 96, 56, DRYING_ACTION_DOWN);
    v->stop_button = drying_button(pg, RI_RACK_STOP, 468, 24, 112, 124, DRYING_ACTION_STOP);
    v->top_bar = plain_bar(pg, 288, DRYING_TOP_Y, 64, 6, DRYING_LINE_DIM);
    v->left_line = plain_bar(pg, 303, DRYING_VERTICAL_Y, 5, 12, DRYING_LINE_DIM);
    v->right_line = plain_bar(pg, 332, DRYING_VERTICAL_Y, 5, 12, DRYING_LINE_DIM);
    v->bottom_bar = plain_bar(pg, 244, DRYING_BOTTOM_MIN_Y, 152, 7, DRYING_LINE_BRIGHT);
    drying_apply_position();
}

void ui_page_devices_set_drying_rack_position(uint8_t percent)
{
    drying_stop_animation(); drying_position_pct = clamp_pct(percent); drying_apply_position();
}

/* ---------- Pager ---------- */

static void pager_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if(c == LV_EVENT_PRESSED || c == LV_EVENT_PRESSING || c == LV_EVENT_SCROLL_BEGIN ||
       c == LV_EVENT_SCROLL || c == LV_EVENT_SCROLL_END || c == LV_EVENT_RELEASED) activity();
    if(c == LV_EVENT_SCROLL_BEGIN && power_on && center != CENTER_TEMP) {
        center = CENTER_TEMP; refresh_aircon();
    }
}

lv_obj_t *ui_page_devices_build(lv_obj_t *parent,
                                ui_devices_activity_cb_t cb,
                                void *ud)
{
    acb = cb; aud = ud; wrapping = false; updating = false;
    temp_axis = GESTURE_PENDING; bath_temp_axis = GESTURE_PENDING;
    aircon_view_n = 0; curtain_view_n = 0; bath_view_n = 0; drying_view_n = 0;

    root = lv_obj_create(parent); lv_obj_remove_style_all(root);
    lv_obj_set_size(root, W, H); lv_obj_set_style_bg_color(root, BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0); lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    pager = lv_obj_create(root); lv_obj_remove_style_all(pager);
    lv_obj_set_size(pager, W, H); lv_obj_set_style_bg_color(pager, BG, 0);
    lv_obj_set_style_bg_opa(pager, LV_OPA_COVER, 0); lv_obj_set_style_pad_all(pager, 0, 0);
    lv_obj_set_scroll_dir(pager, LV_DIR_HOR); lv_obj_set_scroll_snap_x(pager, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_flag(pager, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_flex_flow(pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pager, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    /* Keep only the four real pages on-device. The previous circular pager
     * duplicated drying + aircon views, creating six heavy LVGL pages at once
     * and exhausting real-device heap when entering Devices. */
    build_aircon(pager);
    build_curtain(pager);
    build_bath(pager);
    build_drying(pager);

    lv_obj_add_event_cb(pager, pager_cb, LV_EVENT_ALL, NULL);
    lv_obj_scroll_to_x(pager, 0, LV_ANIM_OFF);
    return root;
}

void ui_page_devices_stop(void)
{
    curtain_stop_animation(); drying_stop_animation();
    root = NULL; pager = NULL; wrapping = false; updating = false;
    temp_axis = GESTURE_PENDING; bath_temp_axis = GESTURE_PENDING;
    aircon_view_n = 0; curtain_view_n = 0; bath_view_n = 0; drying_view_n = 0;
    acb = NULL; aud = NULL;
}
