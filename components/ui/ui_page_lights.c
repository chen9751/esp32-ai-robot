#include "ui_page_lights.h"
#include "ui_lights_labels.h"
#include "ui_lights_icons.h"

#include <stddef.h>
#include <stdint.h>

#define UI_SCREEN_W 640
#define UI_SCREEN_H 172
#define ITEM_W 139
#define ITEM_H 158
#define LEFT_PAD 58
#define RIGHT_PAD 14
#define GAP 5
#define CTRL_Y 116
#define CTRL_H 34

#define C_BG        lv_color_hex(0x000000)
#define C_CARD      lv_color_hex(0x0B0C0E)
#define C_CARD_ON   lv_color_hex(0x101011)
#define C_TEXT      lv_color_hex(0xF5F5F7)
#define C_OFF       lv_color_hex(0x686C73)
#define C_OFF_DARK  lv_color_hex(0x25272B)
#define C_WARM      lv_color_hex(0xFFD37A)
#define C_WARM_HI   lv_color_hex(0xFFF1C4)
#define C_CTRL_BG   lv_color_hex(0x17181B)
#define C_CTRL_EDGE lv_color_hex(0x303238)
#define C_WARM_EDGE lv_color_hex(0x5D4A2A)

typedef enum { LIGHT_NORMAL=0, LIGHT_RGB, LIGHT_SWITCH_ONLY } light_kind_t;

typedef struct {
    const lv_image_dsc_t *label;
    ui_lights_icon_t icon;
    light_kind_t kind;
    bool initial_on;
} light_spec_t;

typedef struct {
    lv_obj_t *item;
    lv_obj_t *label;
    lv_obj_t *accent;
    lv_obj_t *room_icon;
    lv_obj_t *controls[3];
    lv_obj_t *control_icons[3];
    lv_obj_t *switch_dot;
    light_kind_t kind;
    bool on;
} light_view_t;

static const light_spec_t SPECS[8] = {
    { &ui_lights_label_living,        UI_LIGHTS_ICON_SOFA,       LIGHT_NORMAL,      true  },
    { &ui_lights_label_study,         UI_LIGHTS_ICON_COMPUTER,   LIGHT_NORMAL,      false },
    { &ui_lights_label_bedroom,       UI_LIGHTS_ICON_BED,        LIGHT_NORMAL,      true  },
    { &ui_lights_label_bedside,       UI_LIGHTS_ICON_BULB,       LIGHT_RGB,         true  },
    { &ui_lights_label_small_bedroom, UI_LIGHTS_ICON_BED,        LIGHT_NORMAL,      false },
    { &ui_lights_label_rgb_strip,     UI_LIGHTS_ICON_TV,         LIGHT_RGB,         true  },
    { &ui_lights_label_bathroom,      UI_LIGHTS_ICON_DROP,       LIGHT_SWITCH_ONLY, false },
    { &ui_lights_label_balcony,       UI_LIGHTS_ICON_WINDOW,     LIGHT_SWITCH_ONLY, true  },
};

static lv_obj_t *s_root;
static ui_lights_activity_cb_t s_activity_cb;
static void *s_activity_user_data;
static light_view_t s_views[8];
static bool s_labels_ready;

static void note_activity(void)
{
    if (s_activity_cb) s_activity_cb(s_activity_user_data);
}

/* Geometry here is layout decoration/control chrome only. Semantic glyphs are Remix Icon. */
static lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h, int radius, lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static ui_lights_icon_t control_icon_type(int kind)
{
    if (kind == 0) return UI_LIGHTS_ICON_BRIGHTNESS;
    if (kind == 1) return UI_LIGHTS_ICON_TEMPERATURE;
    return UI_LIGHTS_ICON_PALETTE;
}

static lv_obj_t *make_control(lv_obj_t *parent, int x, int kind, lv_obj_t **icon_out)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, CTRL_Y);
    lv_obj_set_size(button, 38, CTRL_H);
    lv_obj_set_style_bg_color(button, C_CTRL_BG, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, C_CTRL_EDGE, 0);
    lv_obj_set_style_radius(button, 11, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = ui_lights_icon_create(button, control_icon_type(kind), C_TEXT);
    lv_obj_center(icon);
    if (icon_out) *icon_out = icon;
    return button;
}

static void create_switch_footer(light_view_t *view)
{
    lv_obj_t *track = rect(view->item, 45, CTRL_Y + 5, 49, 24, 12, C_CTRL_BG);
    lv_obj_set_style_border_width(track, 1, 0);
    lv_obj_set_style_border_color(track, C_CTRL_EDGE, 0);
    view->switch_dot = rect(track, 4, 4, 16, 16, 8, C_OFF);
}

static void update_state(light_view_t *view)
{
    const lv_color_t main = view->on ? C_WARM_HI : C_OFF;
    const lv_color_t edge = view->on ? C_WARM_EDGE : C_OFF_DARK;

    lv_obj_set_style_bg_color(view->item, view->on ? C_CARD_ON : C_CARD, 0);
    lv_obj_set_style_image_recolor(view->label, view->on ? C_TEXT : C_OFF, 0);
    lv_obj_set_style_image_recolor_opa(view->label, LV_OPA_COVER, 0);

    lv_obj_set_style_bg_color(view->accent, view->on ? C_WARM : C_OFF_DARK, 0);
    lv_obj_set_style_bg_opa(view->accent, view->on ? LV_OPA_COVER : LV_OPA_50, 0);
    lv_obj_set_style_shadow_color(view->accent, view->on ? C_WARM : C_OFF_DARK, 0);
    lv_obj_set_style_shadow_width(view->accent, view->on ? 8 : 0, 0);
    lv_obj_set_style_shadow_opa(view->accent, view->on ? LV_OPA_40 : LV_OPA_TRANSP, 0);

    ui_lights_icon_set_color(view->room_icon, main);
    lv_obj_set_style_opa(view->room_icon, view->on ? LV_OPA_COVER : LV_OPA_60, 0);

    for (int i = 0; i < 3; i++) {
        if (!view->controls[i]) continue;
        lv_obj_set_style_border_color(view->controls[i], edge, 0);
        lv_obj_set_style_bg_color(view->controls[i], view->on ? C_CTRL_BG : C_CARD, 0);
        if (view->control_icons[i]) {
            ui_lights_icon_set_color(view->control_icons[i], main);
            lv_obj_set_style_opa(view->control_icons[i], view->on ? LV_OPA_COVER : LV_OPA_50, 0);
        }
    }

    if (view->switch_dot) {
        lv_obj_t *track = lv_obj_get_parent(view->switch_dot);
        lv_obj_set_style_border_color(track, edge, 0);
        lv_obj_set_style_bg_color(track, view->on ? lv_color_hex(0x342B1D) : C_CTRL_BG, 0);
        lv_obj_set_x(view->switch_dot, view->on ? 29 : 4);
        lv_obj_set_style_bg_color(view->switch_dot, main, 0);
        lv_obj_set_style_bg_opa(view->switch_dot, view->on ? LV_OPA_COVER : LV_OPA_60, 0);
    }
}

static void item_click(lv_event_t *event)
{
    light_view_t *view = (light_view_t *)lv_event_get_user_data(event);
    if (!view) return;
    view->on = !view->on;
    update_state(view);
    note_activity();
}

static void scroll_activity(lv_event_t *event)
{
    (void)event;
    note_activity();
}

static void create_item(lv_obj_t *parent, size_t index)
{
    const light_spec_t *spec = &SPECS[index];
    light_view_t *view = &s_views[index];
    *view = (light_view_t){0};
    view->kind = spec->kind;
    view->on = spec->initial_on;

    view->item = lv_obj_create(parent);
    lv_obj_remove_style_all(view->item);
    lv_obj_set_size(view->item, ITEM_W, ITEM_H);
    lv_obj_set_style_bg_color(view->item, C_CARD, 0);
    lv_obj_set_style_bg_opa(view->item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(view->item, 14, 0);
    lv_obj_set_style_border_width(view->item, 1, 0);
    lv_obj_set_style_border_color(view->item, lv_color_hex(0x191B1F), 0);
    lv_obj_clear_flag(view->item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view->item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(view->item, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(view->item, item_click, LV_EVENT_CLICKED, view);

    view->label = lv_image_create(view->item);
    lv_image_set_src(view->label, spec->label);
    lv_obj_set_style_image_recolor(view->label, C_TEXT, 0);
    lv_obj_set_style_image_recolor_opa(view->label, LV_OPA_COVER, 0);
    lv_obj_align(view->label, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_clear_flag(view->label, LV_OBJ_FLAG_CLICKABLE);

    view->accent = rect(view->item, 43, 37, 53, 3, 2, C_WARM);
    view->room_icon = ui_lights_icon_create(view->item, spec->icon, C_WARM_HI);
    lv_obj_align(view->room_icon, LV_ALIGN_TOP_MID, 0, 57);

    if (spec->kind == LIGHT_SWITCH_ONLY) {
        create_switch_footer(view);
    } else if (spec->kind == LIGHT_NORMAL) {
        view->controls[0] = make_control(view->item, 28, 0, &view->control_icons[0]);
        view->controls[1] = make_control(view->item, 73, 1, &view->control_icons[1]);
    } else {
        view->controls[0] = make_control(view->item, 8, 0, &view->control_icons[0]);
        view->controls[1] = make_control(view->item, 51, 1, &view->control_icons[1]);
        view->controls[2] = make_control(view->item, 94, 2, &view->control_icons[2]);
    }

    update_state(view);
}

lv_obj_t *ui_page_lights_build(lv_obj_t *parent,
                               ui_lights_activity_cb_t activity_cb,
                               void *activity_user_data)
{
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    if (!s_labels_ready) {
        ui_lights_labels_init();
        s_labels_ready = true;
    }

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(s_root, C_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *scroller = lv_obj_create(s_root);
    lv_obj_remove_style_all(scroller);
    lv_obj_set_size(scroller, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(scroller, C_BG, 0);
    lv_obj_set_style_bg_opa(scroller, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(scroller, LEFT_PAD, 0);
    lv_obj_set_style_pad_right(scroller, RIGHT_PAD, 0);
    lv_obj_set_style_pad_column(scroller, GAP, 0);
    lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scroller, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(scroller, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_event_cb(scroller, scroll_activity, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(scroller, scroll_activity, LV_EVENT_SCROLL, NULL);

    for (size_t i = 0; i < 8; i++) create_item(scroller, i);
    return s_root;
}

void ui_page_lights_stop(void)
{
    if (s_root) lv_obj_delete(s_root);
    s_root = NULL;
    s_activity_cb = NULL;
    s_activity_user_data = NULL;
}
