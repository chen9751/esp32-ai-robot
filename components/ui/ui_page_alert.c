#include "ui_page_alert.h"
#include "ui_assets.h"

#define UI_SCREEN_W 640
#define UI_SCREEN_H 172
#define UI_COLOR_BG     lv_color_hex(0x000000)
#define UI_COLOR_ACCENT lv_color_hex(0x45D7F0)
#define UI_COLOR_FG     lv_color_hex(0xFFFFFF)
#define UI_COLOR_DIM    lv_color_hex(0x17333A)

static lv_obj_t *s_root = NULL;
static ui_alert_dismiss_cb_t s_dismiss_cb = NULL;
static void *s_dismiss_user_data = NULL;

static void dismiss_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
    if (s_dismiss_cb != NULL) s_dismiss_cb(s_dismiss_user_data);
}

lv_obj_t *ui_page_alert_build(lv_obj_t *parent,
                              ui_alert_dismiss_cb_t dismiss_cb,
                              void *user_data)
{
    ui_page_alert_stop();

    s_dismiss_cb = dismiss_cb;
    s_dismiss_user_data = user_data;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_root, dismiss_event_cb, LV_EVENT_PRESSED, NULL);

    /* Keep the ringing screen visually light. The previous oversized cyan
     * recolor made the alarm glyph read as a heavy solid blob on this very
     * short 172px display. Use a white Remix alarm glyph at a smaller scale,
     * framed by a thin cyan halo and matching cyan ringing marks. */
    lv_obj_t *halo = lv_obj_create(s_root);
    lv_obj_remove_style_all(halo);
    lv_obj_set_size(halo, 118, 118);
    lv_obj_set_style_bg_opa(halo, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(halo, 2, 0);
    lv_obj_set_style_border_color(halo, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_opa(halo, LV_OPA_50, 0);
    lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(halo, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(halo, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *inner = lv_obj_create(s_root);
    lv_obj_remove_style_all(inner);
    lv_obj_set_size(inner, 98, 98);
    lv_obj_set_style_bg_color(inner, UI_COLOR_DIM, 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_30, 0);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(inner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(inner, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon = lv_image_create(s_root);
    lv_image_set_src(icon, &ui_icon_alarm);
    lv_obj_set_style_image_recolor(icon, UI_COLOR_FG, 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_image_set_scale(icon, 390); /* 58px source -> ~88px visual size */
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *left = lv_label_create(s_root);
    lv_label_set_text(left, "((");
    lv_obj_set_style_text_font(left, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(left, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_opa(left, LV_OPA_80, 0);
    lv_obj_align(left, LV_ALIGN_CENTER, -142, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *right = lv_label_create(s_root);
    lv_label_set_text(right, "))");
    lv_obj_set_style_text_font(right, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(right, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_opa(right, LV_OPA_80, 0);
    lv_obj_align(right, LV_ALIGN_CENTER, 142, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_move_foreground(s_root);
    return s_root;
}

void ui_page_alert_stop(void)
{
    if (s_root != NULL) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
    s_dismiss_cb = NULL;
    s_dismiss_user_data = NULL;
}

bool ui_page_alert_active(void)
{
    return s_root != NULL;
}
