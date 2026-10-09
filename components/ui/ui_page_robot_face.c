#include "ui_page_robot_face.h"
#include "lvgl_kawaii_face.h"
#if defined(ESP_PLATFORM)
#include "esp_random.h"
#endif

static bool s_kawaii_active = false;
/* All face APIs are invoked from the LVGL-owned UI context. Avoid recursive
 * acquisition of the board display mutex if esp_lvgl_port is present. */
static void face_noop_lock(void) {}
static void face_noop_unlock(void) {}

#define FACE_W 640
#define FACE_H 172
#define FACE_BG lv_color_hex(0x000000)
#define FACE_CYAN lv_color_hex(0x49E9F0)
#define FACE_WHITE lv_color_hex(0xF5FAFF)

static lv_obj_t *s_face = NULL;
static lv_timer_t *s_pulse_timer = NULL;
static lv_obj_t *s_waves[6];
static ui_robot_face_mode_t s_mode = UI_ROBOT_FACE_SMILE;
static uint8_t s_pulse = 0;

/* Test mode: pick among the 17 emotions; FACE_BLINK is a transient state. */
static face_emotion_t random_wakeup_emotion(void)
{
#if defined(ESP_PLATFORM)
    return (face_emotion_t)(esp_random() % (uint32_t)(FACE_COOL + 1));
#else
    return (face_emotion_t)lv_rand(0, FACE_COOL);
#endif
}


static lv_obj_t *pill(lv_obj_t *parent, int32_t x, int32_t y,
                      int32_t w, int32_t h, lv_color_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void pulse_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_face == NULL || s_mode != UI_ROBOT_FACE_LISTENING) return;
    ++s_pulse;
    for (int i = 0; i < 6; ++i) {
        if (s_waves[i] != NULL) {
            uint8_t intensity = (uint8_t)((s_pulse + (i % 3)) % 5);
            lv_obj_set_style_bg_opa(s_waves[i],
                                    (lv_opa_t)(85 + intensity * 40), 0);
        }
    }
}

void ui_page_robot_face_stop(void)
{
    if (s_kawaii_active) {
        face_animation_deinit();
        s_kawaii_active = false;
    }
    if (s_pulse_timer != NULL) {
        lv_timer_delete(s_pulse_timer);
        s_pulse_timer = NULL;
    }
    for (int i = 0; i < 6; ++i) s_waves[i] = NULL;
    if (s_face != NULL) lv_obj_delete(s_face);
    s_face = NULL;
}

void ui_page_robot_face_set_mode(ui_robot_face_mode_t mode)
{
    s_mode = mode;
    if (s_face == NULL) return;
    if (s_kawaii_active) {
        face_set_emotion(mode == UI_ROBOT_FACE_LISTENING ? random_wakeup_emotion() : FACE_NEUTRAL, true);
        return;
    }
    for (int i = 0; i < 6; ++i) {
        if (s_waves[i] == NULL) continue;
        if (mode == UI_ROBOT_FACE_LISTENING)
            lv_obj_remove_flag(s_waves[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_waves[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (mode == UI_ROBOT_FACE_LISTENING && s_pulse_timer == NULL)
        s_pulse_timer = lv_timer_create(pulse_cb, 180, NULL);
    else if (mode != UI_ROBOT_FACE_LISTENING && s_pulse_timer != NULL) {
        lv_timer_delete(s_pulse_timer);
        s_pulse_timer = NULL;
    }
}

lv_obj_t *ui_page_robot_face_build(lv_obj_t *parent, ui_robot_face_mode_t mode)
{
    ui_page_robot_face_stop();
    if (parent == NULL) return NULL;

    s_face = lv_obj_create(parent);
    lv_obj_null_on_delete(&s_face);
    lv_obj_remove_style_all(s_face);
    lv_obj_set_size(s_face, FACE_W, FACE_H);
    lv_obj_set_pos(s_face, 0, 0);
    lv_obj_set_style_bg_color(s_face, FACE_BG, 0);
    lv_obj_set_style_bg_opa(s_face, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_face, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_face, LV_OBJ_FLAG_SCROLLABLE);

    /* The engine is confined to 160x160 within the 640x172 canvas so that
     * its three RGB565 canvases stay small (~29 KB) and the clock lock
     * overlay can continue to sit above it. The entire widget remains
     * replaceable by the existing LVGL geometry below on allocation failure. */
    lv_obj_t *panel = lv_obj_create(s_face);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 160, 160);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    face_config_t cfg = {
        .parent = panel,
        .animation_speed = 50, /* 20 FPS reduces display/CPU pressure. */
        .blink_interval = 3000,
        .auto_blink = true,
    };
    face_set_lvgl_lock_fns(face_noop_lock, face_noop_unlock);
    if (face_animation_init(&cfg) == ESP_OK) {
        s_kawaii_active = true;
        face_set_emotion(mode == UI_ROBOT_FACE_LISTENING ? random_wakeup_emotion() : FACE_NEUTRAL, false);
        return s_face;
    }
    lv_obj_delete(panel);

    /* Continuous antialiased smile: one LVGL arc instead of five pills.
     * LVGL arc angles 25..155 describe the downward-facing lower semicircle.
     * Hide the background track and knob so only the smooth smile is drawn. */
    pill(s_face, 239, 49, 24, 42, FACE_WHITE);
    pill(s_face, 377, 49, 24, 42, FACE_WHITE);
    lv_obj_t *smile = lv_arc_create(s_face);
    lv_obj_remove_style_all(smile);
    lv_obj_set_size(smile, 70, 70);
    lv_obj_set_pos(smile, 285, 56);
    lv_arc_set_rotation(smile, 0);
    lv_arc_set_bg_angles(smile, 0, 0);
    lv_arc_set_angles(smile, 25, 155);
    lv_obj_set_style_arc_width(smile, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(smile, FACE_WHITE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(smile, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(smile, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(smile, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(smile, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(smile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(smile, LV_OBJ_FLAG_SCROLLABLE);

    /* Three sound-level bars on each side, animated only while listening. */
    const int32_t xs[6] = {142, 163, 184, 444, 465, 486};
    const int32_t heights[6] = {25, 50, 35, 35, 50, 25};
    for (int i = 0; i < 6; ++i) {
        s_waves[i] = pill(s_face, xs[i], (FACE_H - heights[i]) / 2,
                          7, heights[i], FACE_CYAN);
    }
    ui_page_robot_face_set_mode(mode);
    return s_face;
}
