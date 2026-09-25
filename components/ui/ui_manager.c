#include "ui_manager.h"

#include <stddef.h>
#include <stdint.h>

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172

/*
 * 640x172 main-menu geometry.
 * Cards remain large and square, but with more breathing room than the
 * previous almost-edge-to-edge version.
 */
#define UI_MENU_ITEM_W       146
#define UI_MENU_ITEM_H       146
#define UI_MENU_GAP          20
#define UI_MENU_SIDE_PAD     18
#define UI_MENU_V_PAD        13
#define UI_MENU_RADIUS       24

#define UI_COLOR_BG          lv_color_hex(0x000000)
#define UI_COLOR_FG          lv_color_hex(0xFFFFFF)

typedef struct {
    const char *label;
    const char *symbol;
    ui_menu_action_t action;
    uint32_t color_top;
    uint32_t color_bottom;
} ui_menu_item_t;

static ui_menu_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static const lv_font_t *s_menu_font = NULL;

/*
 * Use LVGL's built-in FontAwesome symbols instead of enlarged bitmap pixels.
 * They are anti-aliased by the font renderer and stay clean at this size.
 */
static const ui_menu_item_t MENU_ITEMS[] = {
    { "REMOTE",   LV_SYMBOL_WIFI,     UI_MENU_REMOTE,   0x19B9F6u, 0x087CF0u },
    { "MUSIC",    LV_SYMBOL_AUDIO,    UI_MENU_MUSIC,    0xFF5F7Bu, 0xC72B69u },
    { "LIGHTS",   LV_SYMBOL_CHARGE,   UI_MENU_LIGHTS,   0xFFD568u, 0xFF933Eu },
    { "DEVICES",  LV_SYMBOL_HOME,     UI_MENU_DEVICES,  0x43D9B1u, 0x08A77Fu },
    { "ALARM",    LV_SYMBOL_BELL,     UI_MENU_ALARM,    0x9165FFu, 0x5E26E9u },
    { "SETTINGS", LV_SYMBOL_SETTINGS, UI_MENU_SETTINGS, 0xA6B3CFu, 0x556487u },
};

static void menu_item_clicked(lv_event_t *e)
{
    const ui_menu_item_t *item =
        (const ui_menu_item_t *)lv_event_get_user_data(e);

    if (item != NULL && s_action_cb != NULL) {
        s_action_cb(item->action, s_action_user_data);
    }
}

static void style_menu_label(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, UI_COLOR_FG, 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);

    if (s_menu_font != NULL) {
        lv_obj_set_style_text_font(label, s_menu_font, 0);
    } else {
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    }
}

static void create_icon(lv_obj_t *parent, const char *symbol)
{
    lv_obj_t *holder = lv_obj_create(parent);
    lv_obj_remove_style_all(holder);
    lv_obj_set_size(holder, 92, 82);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon = lv_label_create(holder);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, UI_COLOR_FG, 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, 1);
}

static void create_menu_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *holder = lv_obj_create(parent);
    lv_obj_remove_style_all(holder);
    lv_obj_set_size(holder, UI_MENU_ITEM_W - 12, 24);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(holder);
    lv_label_set_text(label, text);
    style_menu_label(label);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

static lv_obj_t *create_menu_item(lv_obj_t *parent, const ui_menu_item_t *item)
{
    lv_obj_t *tile = lv_obj_create(parent);

    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, UI_MENU_ITEM_W, UI_MENU_ITEM_H);

    lv_obj_set_style_bg_color(tile, lv_color_hex(item->color_top), 0);
    lv_obj_set_style_bg_grad_color(tile, lv_color_hex(item->color_bottom), 0);
    lv_obj_set_style_bg_grad_dir(tile, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tile, UI_MENU_RADIUS, 0);
    lv_obj_set_style_border_width(tile, 0, 0);

    /*
     * Keep icon and text visually centered as one group.
     * The larger row gap prevents the label from crowding the icon.
     */
    lv_obj_set_style_pad_top(tile, 12, 0);
    lv_obj_set_style_pad_bottom(tile, 9, 0);
    lv_obj_set_style_pad_left(tile, 0, 0);
    lv_obj_set_style_pad_right(tile, 0, 0);
    lv_obj_set_style_pad_row(tile, 8, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    create_icon(tile, item->symbol);
    create_menu_label(tile, item->label);

    lv_obj_add_event_cb(tile,
                        menu_item_clicked,
                        LV_EVENT_CLICKED,
                        (void *)item);
    return tile;
}

void ui_set_menu_action_cb(ui_menu_action_cb_t cb, void *user_data)
{
    s_action_cb = cb;
    s_action_user_data = user_data;
}

void ui_set_menu_font(const lv_font_t *font)
{
    s_menu_font = font;
}

void ui_show_main_menu(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);

    lv_obj_set_size(screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(screen, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *scroller = lv_obj_create(screen);
    lv_obj_remove_style_all(scroller);
    lv_obj_set_size(scroller, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_center(scroller);

    lv_obj_set_style_bg_color(scroller, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scroller, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(scroller, UI_MENU_SIDE_PAD, 0);
    lv_obj_set_style_pad_right(scroller, UI_MENU_SIDE_PAD, 0);
    lv_obj_set_style_pad_top(scroller, UI_MENU_V_PAD, 0);
    lv_obj_set_style_pad_bottom(scroller, UI_MENU_V_PAD, 0);
    lv_obj_set_style_pad_column(scroller, UI_MENU_GAP, 0);

    lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scroller,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_set_scroll_dir(scroller, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(scroller, LV_SCROLL_SNAP_START);
    lv_obj_add_flag(scroller, LV_OBJ_FLAG_SCROLL_ONE);

    for (size_t i = 0;
         i < sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
         ++i) {
        create_menu_item(scroller, &MENU_ITEMS[i]);
    }
}

void ui_init(void)
{
    ui_show_main_menu();
}
