#include "ui_page_music.h"

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172
#define UI_BACK_RAIL_W        56

#define UI_COLOR_BG       lv_color_hex(0x000000)
#define UI_COLOR_FG       lv_color_hex(0xF5F7F8)
#define UI_COLOR_MUTED    lv_color_hex(0x8C9197)
#define UI_COLOR_TRACK    lv_color_hex(0x3A3D41)
#define UI_COLOR_PANEL    lv_color_hex(0x101719)
#define UI_COLOR_ACCENT   lv_color_hex(0x38F2B4)
#define UI_COLOR_BORDER   lv_color_hex(0x263033)

static ui_music_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static ui_music_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_subtitle = NULL;
static lv_obj_t *s_progress = NULL;
static lv_obj_t *s_elapsed = NULL;
static lv_obj_t *s_duration = NULL;
static lv_obj_t *s_play_label = NULL;
static lv_obj_t *s_target_tv = NULL;
static lv_obj_t *s_target_speaker = NULL;

static ui_music_target_t s_target = UI_MUSIC_TARGET_TV;
static bool s_playing = false;
static int32_t s_position_seconds = 84;
static int32_t s_duration_seconds = 276;

static void note_activity(void)
{
    if (s_activity_cb != NULL) s_activity_cb(s_activity_user_data);
}

static void emit_action(ui_music_action_t action, int32_t value)
{
    note_activity();
    if (s_action_cb != NULL) s_action_cb(action, value, s_action_user_data);
}

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

static void format_time(char *buf, size_t buf_size, int32_t seconds)
{
    if (seconds < 0) seconds = 0;
    int32_t minutes = seconds / 60;
    int32_t secs = seconds % 60;
    lv_snprintf(buf, buf_size, "%02ld:%02ld", (long)minutes, (long)secs);
}

static void refresh_time_labels(void)
{
    char elapsed[16];
    char duration[16];
    format_time(elapsed, sizeof(elapsed), s_position_seconds);
    format_time(duration, sizeof(duration), s_duration_seconds);
    if (s_elapsed != NULL) lv_label_set_text(s_elapsed, elapsed);
    if (s_duration != NULL) lv_label_set_text(s_duration, duration);
}

static void refresh_play_icon(void)
{
    if (s_play_label != NULL) {
        lv_label_set_text(s_play_label, s_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
}

static void style_target_button(lv_obj_t *btn, bool active)
{
    if (btn == NULL) return;

    lv_obj_set_style_bg_color(btn, active ? UI_COLOR_ACCENT : UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(btn, active ? 0 : 1, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);

    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label != NULL) {
        lv_obj_set_style_text_color(label,
                                    active ? lv_color_hex(0x07140F) : UI_COLOR_FG,
                                    0);
    }
}

static void refresh_target(void)
{
    style_target_button(s_target_tv, s_target == UI_MUSIC_TARGET_TV);
    style_target_button(s_target_speaker, s_target == UI_MUSIC_TARGET_SPEAKER);
}

static lv_obj_t *make_target_button(lv_obj_t *parent,
                                    int32_t x,
                                    int32_t width,
                                    const char *text,
                                    const lv_font_t *font)
{
    lv_obj_t *btn = plain_obj(parent);
    lv_obj_set_size(btn, width, 34);
    lv_obj_set_pos(btn, x, 17);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = make_label(btn, text, UI_COLOR_FG, font);
    lv_obj_center(label);
    return btn;
}

static lv_obj_t *make_transport_button(lv_obj_t *parent,
                                       int32_t x,
                                       int32_t y,
                                       int32_t size,
                                       const char *symbol,
                                       bool primary)
{
    lv_obj_t *btn = plain_obj(parent);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, primary ? UI_COLOR_ACCENT : UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(btn, primary ? 0 : 1, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = make_label(btn, symbol,
                                 primary ? lv_color_hex(0x07140F) : UI_COLOR_FG,
                                 &lv_font_montserrat_20);
    lv_obj_center(label);
    return btn;
}

static void target_event_cb(lv_event_t *e)
{
    lv_obj_t *target_obj = lv_event_get_target(e);
    ui_music_target_t next = target_obj == s_target_speaker
                           ? UI_MUSIC_TARGET_SPEAKER
                           : UI_MUSIC_TARGET_TV;
    if (next == s_target) {
        note_activity();
        return;
    }
    s_target = next;
    refresh_target();
    emit_action(UI_MUSIC_ACTION_TARGET_CHANGED, (int32_t)s_target);
}

static void prev_event_cb(lv_event_t *e)
{
    (void)e;
    emit_action(UI_MUSIC_ACTION_PREVIOUS, 0);
}

static void play_event_cb(lv_event_t *e)
{
    (void)e;
    s_playing = !s_playing;
    refresh_play_icon();
    emit_action(UI_MUSIC_ACTION_PLAY_PAUSE, s_playing ? 1 : 0);
}

static void next_event_cb(lv_event_t *e)
{
    (void)e;
    emit_action(UI_MUSIC_ACTION_NEXT, 0);
}

void ui_page_music_build(lv_obj_t *parent,
                         ui_music_activity_cb_t activity_cb,
                         void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;

    s_root = plain_obj(parent);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    s_title = make_label(s_root, "No track", UI_COLOR_FG, &lv_font_montserrat_28);
    lv_obj_set_pos(s_title, 78, 22);
    lv_obj_set_width(s_title, 300);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_DOT);

    s_subtitle = make_label(s_root, "Waiting for media", UI_COLOR_MUTED,
                            &lv_font_montserrat_16);
    lv_obj_set_pos(s_subtitle, 78, 56);
    lv_obj_set_width(s_subtitle, 300);
    lv_label_set_long_mode(s_subtitle, LV_LABEL_LONG_DOT);

    /* Target buttons deliberately share the same visual language as transport:
     * accent = active, dark circular/pill surface = inactive.  They are kept
     * as separate controls instead of nesting two pills inside one container. */
    s_target_tv = make_target_button(s_root, 432, 72, "TV", &lv_font_montserrat_16);
    lv_obj_add_event_cb(s_target_tv, target_event_cb, LV_EVENT_CLICKED, NULL);

    s_target_speaker = make_target_button(s_root, 512, 110, "Speaker", &lv_font_montserrat_14);
    lv_obj_add_event_cb(s_target_speaker, target_event_cb, LV_EVENT_CLICKED, NULL);
    refresh_target();

    /* Display-only progress.  lv_bar has no knob and is not clickable, so
     * playback position can only be updated by ui_page_music_set_playback(). */
    s_progress = lv_bar_create(s_root);
    lv_obj_set_size(s_progress, 326, 8);
    lv_obj_set_pos(s_progress, 78, 106);
    lv_bar_set_range(s_progress, 0, 1000);
    lv_obj_clear_flag(s_progress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(s_progress, UI_COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_progress, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_progress, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    s_elapsed = make_label(s_root, "00:00", UI_COLOR_MUTED, &lv_font_montserrat_14);
    lv_obj_set_pos(s_elapsed, 78, 127);
    s_duration = make_label(s_root, "00:00", UI_COLOR_MUTED, &lv_font_montserrat_14);
    lv_obj_set_pos(s_duration, 358, 127);

    lv_obj_t *prev = make_transport_button(s_root, 445, 78, 52, LV_SYMBOL_PREV, false);
    lv_obj_add_event_cb(prev, prev_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *play = make_transport_button(s_root, 505, 69, 70,
                                           s_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY,
                                           true);
    s_play_label = lv_obj_get_child(play, 0);
    lv_obj_add_event_cb(play, play_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next = make_transport_button(s_root, 583, 78, 52, LV_SYMBOL_NEXT, false);
    lv_obj_add_event_cb(next, next_event_cb, LV_EVENT_CLICKED, NULL);

    ui_page_music_set_playback(s_playing, s_position_seconds, s_duration_seconds);
}

void ui_page_music_stop(void)
{
    s_root = NULL;
    s_title = NULL;
    s_subtitle = NULL;
    s_progress = NULL;
    s_elapsed = NULL;
    s_duration = NULL;
    s_play_label = NULL;
    s_target_tv = NULL;
    s_target_speaker = NULL;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}

void ui_page_music_set_action_cb(ui_music_action_cb_t cb, void *user_data)
{
    s_action_cb = cb;
    s_action_user_data = user_data;
}

void ui_page_music_set_target(ui_music_target_t target)
{
    s_target = target;
    refresh_target();
}

ui_music_target_t ui_page_music_get_target(void)
{
    return s_target;
}

void ui_page_music_set_metadata(const char *title, const char *subtitle)
{
    if (s_title != NULL) lv_label_set_text(s_title, title != NULL ? title : "No track");
    if (s_subtitle != NULL) lv_label_set_text(s_subtitle,
                                               subtitle != NULL ? subtitle : "");
}

void ui_page_music_set_playback(bool playing,
                                int32_t position_seconds,
                                int32_t duration_seconds)
{
    s_playing = playing;
    s_duration_seconds = duration_seconds < 0 ? 0 : duration_seconds;
    s_position_seconds = position_seconds < 0 ? 0 : position_seconds;
    if (s_duration_seconds > 0 && s_position_seconds > s_duration_seconds) {
        s_position_seconds = s_duration_seconds;
    }

    refresh_play_icon();
    refresh_time_labels();

    if (s_progress != NULL) {
        int32_t value = 0;
        if (s_duration_seconds > 0) {
            value = (s_position_seconds * 1000) / s_duration_seconds;
        }
        lv_bar_set_value(s_progress, value, LV_ANIM_OFF);
    }
}
