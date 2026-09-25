#include "ui_manager.h"
#include "ui_assets.h"

#include <stddef.h>
#include <stdint.h>

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172
#define UI_MENU_ITEM_W       136
#define UI_MENU_ITEM_H       136
#define UI_MENU_GAP          22
#define UI_MENU_SIDE_PAD     18
#define UI_MENU_V_PAD        18
#define UI_MENU_RADIUS       30

#define UI_COLOR_BG          lv_color_hex(0x000000)
#define UI_COLOR_FG          lv_color_hex(0xFFFFFF)

typedef struct {
    const lv_image_dsc_t *icon;
    const lv_image_dsc_t *label_image;
    ui_menu_action_t action;
    uint32_t color_top;
    uint32_t color_bottom;
} ui_menu_item_t;

static ui_menu_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static const lv_font_t *s_menu_font = NULL;

static const ui_menu_item_t MENU_ITEMS[] = {
    { &ui_icon_remote,   &ui_label_remote,   UI_MENU_REMOTE,   0x31B9FFu, 0x075EF0u },
    { &ui_icon_music,    &ui_label_music,    UI_MENU_MUSIC,    0xFF687Eu, 0xC92D70u },
    { &ui_icon_light,    &ui_label_light,    UI_MENU_LIGHTS,   0xFFD66Au, 0xFF913Eu },
    { &ui_icon_devices,  &ui_label_devices,  UI_MENU_DEVICES,  0x48D8B1u, 0x0AA57Fu },
    { &ui_icon_alarm,    &ui_label_alarm,    UI_MENU_ALARM,    0x9A68FFu, 0x5D28EBu },
    { &ui_icon_settings, &ui_label_settings, UI_MENU_SETTINGS, 0xA9B8D2u, 0x596985u },
};

static void menu_item_clicked(lv_event_t *e)
{
    const ui_menu_item_t *item = (const ui_menu_item_t *)lv_event_get_user_data(e);

    if (item != NULL && s_action_cb != NULL) {
        s_action_cb(item->action, s_action_user_data);
    }
}

static lv_obj_t *create_a8_image(lv_obj_t *parent, const lv_image_dsc_t *src)
{
    lv_obj_t *image = lv_image_create(parent);
    lv_image_set_src(image, src);
    lv_obj_set_style_image_recolor(image, UI_COLOR_FG, 0);
    lv_obj_set_style_image_recolor_opa(image, LV_OPA_COVER, 0);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    return image;
}

static void create_icon(lv_obj_t *parent, const lv_image_dsc_t *src)
{
    lv_obj_t *holder = lv_obj_create(parent);
    lv_obj_remove_style_all(holder);
    lv_obj_set_size(holder, 96, 78);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon = create_a8_image(holder, src);
    /*
     * Keep the A8 icon at its native 58 x 58 size.
     * Scaling a raster mask by 288/256 (~1.125x) introduced a second
     * resampling pass and made the Remix glyph edges visibly soft.
     * Native-size rendering preserves the SVG rasterizer's antialiasing.
     */
    lv_image_set_scale(icon, LV_SCALE_NONE);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -1);
}

static void create_menu_label(lv_obj_t *parent, const lv_image_dsc_t *src)
{
    lv_obj_t *holder = lv_obj_create(parent);
    lv_obj_remove_style_all(holder);
    lv_obj_set_size(holder, UI_MENU_ITEM_W - 10, 24);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = create_a8_image(holder, src);
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

    lv_obj_set_style_pad_top(tile, 10, 0);
    lv_obj_set_style_pad_bottom(tile, 8, 0);
    lv_obj_set_style_pad_left(tile, 0, 0);
    lv_obj_set_style_pad_right(tile, 0, 0);
    lv_obj_set_style_pad_row(tile, 6, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    create_icon(tile, item->icon);
    create_menu_label(tile, item->label_image);

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
    /*
     * Kept for API compatibility. The fixed homepage labels are pre-rendered
     * A8 assets; dynamic CJK pages can use the configured text font later.
     */
    s_menu_font = font;
    (void)s_menu_font;
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
    /* Hard-clamp the carousel to its real content width.  Start snapping
     * made the last card snap to the left edge, leaving a large empty tail. */
    lv_obj_set_scroll_snap_x(scroller, LV_SCROLL_SNAP_NONE);
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLL_ELASTIC);

    for (size_t i = 0; i < sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]); ++i) {
        create_menu_item(scroller, &MENU_ITEMS[i]);
    }
}

void ui_init(void)
{
    ui_assets_init();
    ui_show_main_menu();
}
