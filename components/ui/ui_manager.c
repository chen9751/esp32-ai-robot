#include "ui_manager.h"

#include <stddef.h>
#include <stdint.h>

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172

/*
 * Use a denser source grid instead of simply enlarging a 16x16 icon.
 * 24x24 x 3 gives a 72x72 on-screen icon while retaining finer pixel detail.
 */
#define UI_ICON_GRID         24
#define UI_MENU_ICON_SCALE   3
#define UI_MENU_ICON_SIZE    (UI_ICON_GRID * UI_MENU_ICON_SCALE)

#define UI_MENU_ITEM_W       146
#define UI_MENU_ITEM_H       136
#define UI_MENU_GAP          30
#define UI_MENU_SIDE_PAD     28

#define UI_COLOR_BG          lv_color_hex(0x050909)
#define UI_COLOR_FG          lv_color_hex(0x69D8C8)

typedef struct {
    const char *label;
    ui_menu_action_t action;
    const uint32_t *bitmap;
} ui_menu_item_t;

static ui_menu_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static const lv_font_t *s_menu_font = NULL;

/*
 * 24x24 monochrome pixel icons.
 * Bit 23 is the left-most pixel.
 */
static const uint32_t ICON_REMOTE[24] = {
    0x000000u, 0x003C00u, 0x00FF00u, 0x01C380u,
    0x0300C0u, 0x000000u, 0x01FF80u, 0x0300C0u,
    0x0318C0u, 0x033CC0u, 0x0318C0u, 0x0300C0u,
    0x0336C0u, 0x0336C0u, 0x0300C0u, 0x0336C0u,
    0x0336C0u, 0x0300C0u, 0x0336C0u, 0x0336C0u,
    0x0300C0u, 0x01FF80u, 0x000000u, 0x000000u,
};

static const uint32_t ICON_MUSIC[24] = {
    0x000000u, 0x007F80u, 0x00C180u, 0x00C180u,
    0x00C180u, 0x00C180u, 0x00C1E0u, 0x00C1E0u,
    0x00C1B0u, 0x00C198u, 0x00C198u, 0x00C198u,
    0x1FC198u, 0x3FC198u, 0x71C198u, 0x61C198u,
    0x61C198u, 0x73C198u, 0x3F0098u, 0x1E0018u,
    0x000038u, 0x000070u, 0x000000u, 0x000000u,
};

static const uint32_t ICON_LIGHT[24] = {
    0x001800u, 0x001800u, 0x181818u, 0x0C1830u,
    0x060060u, 0x007E00u, 0x01FF80u, 0x0381C0u,
    0x0700E0u, 0x060060u, 0x060060u, 0x0300C0u,
    0x018180u, 0x00C300u, 0x007E00u, 0x003C00u,
    0x003C00u, 0x007E00u, 0x007E00u, 0x003C00u,
    0x003C00u, 0x000000u, 0x000000u, 0x000000u,
};

static const uint32_t ICON_DEVICE[24] = {
    0x000000u, 0x001800u, 0x003C00u, 0x007E00u,
    0x00FF00u, 0x01FF80u, 0x03FFC0u, 0x07FFE0u,
    0x0FFFF0u, 0x1F807Eu, 0x3E001Fu, 0x3C000Fu,
    0x3C000Fu, 0x3C3F0Fu, 0x3C3F0Fu, 0x3C3F0Fu,
    0x3C3F0Fu, 0x3C3F0Fu, 0x3C3F0Fu, 0x3C000Fu,
    0x3FFFFCu, 0x3FFFFCu, 0x000000u, 0x000000u,
};

static const uint32_t ICON_ALARM[24] = {
    0x0F003Cu, 0x1F807Eu, 0x39C0E7u, 0x000000u,
    0x00FF00u, 0x03FFC0u, 0x0781E0u, 0x0E0070u,
    0x1C0038u, 0x181818u, 0x38181Cu, 0x30180Cu,
    0x301F8Cu, 0x301F8Cu, 0x38001Cu, 0x180018u,
    0x1C0038u, 0x0E0070u, 0x0781E0u, 0x03FFC0u,
    0x00FF00u, 0x0381C0u, 0x0700E0u, 0x000000u,
};

static const uint32_t ICON_SETTINGS[24] = {
    0x003C00u, 0x003C00u, 0x0C3C30u, 0x1E3C78u,
    0x1FFFF8u, 0x0FFFF0u, 0x03FF00u, 0x078780u,
    0x3F03F0u, 0x7E01F8u, 0x780078u, 0xF07E0Fu,
    0xF07E0Fu, 0x780078u, 0x7E01F8u, 0x3F03F0u,
    0x078780u, 0x03FF00u, 0x0FFFF0u, 0x1FFFF8u,
    0x1E3C78u, 0x0C3C30u, 0x003C00u, 0x003C00u,
};

static const ui_menu_item_t MENU_ITEMS[] = {
    { "REMOTE",   UI_MENU_REMOTE,   ICON_REMOTE   },
    { "MUSIC",    UI_MENU_MUSIC,    ICON_MUSIC    },
    { "LIGHTS",   UI_MENU_LIGHTS,   ICON_LIGHT    },
    { "DEVICES",  UI_MENU_DEVICES,  ICON_DEVICE   },
    { "ALARM",    UI_MENU_ALARM,    ICON_ALARM    },
    { "SETTINGS", UI_MENU_SETTINGS, ICON_SETTINGS },
};

static void menu_item_clicked(lv_event_t *e)
{
    const ui_menu_item_t *item =
        (const ui_menu_item_t *)lv_event_get_user_data(e);

    if (item != NULL && s_action_cb != NULL) {
        s_action_cb(item->action, s_action_user_data);
    }
}

static void draw_pixel_bitmap(lv_obj_t *parent, const uint32_t bitmap[UI_ICON_GRID])
{
    lv_obj_t *pixel_layer = lv_obj_create(parent);
    lv_obj_remove_style_all(pixel_layer);
    lv_obj_set_size(pixel_layer, UI_MENU_ICON_SIZE, UI_MENU_ICON_SIZE);
    lv_obj_set_style_bg_opa(pixel_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(pixel_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(pixel_layer, LV_OBJ_FLAG_CLICKABLE);

    for (int y = 0; y < UI_ICON_GRID; ++y) {
        uint32_t row = bitmap[y];
        int x = 0;

        while (x < UI_ICON_GRID) {
            while (x < UI_ICON_GRID &&
                   ((row & (0x800000u >> x)) == 0u)) {
                ++x;
            }

            if (x >= UI_ICON_GRID) {
                break;
            }

            const int start = x;

            while (x < UI_ICON_GRID &&
                   (row & (0x800000u >> x)) != 0u) {
                ++x;
            }

            const int run = x - start;
            lv_obj_t *bar = lv_obj_create(pixel_layer);
            lv_obj_remove_style_all(bar);
            lv_obj_set_pos(bar,
                           start * UI_MENU_ICON_SCALE,
                           y * UI_MENU_ICON_SCALE);
            lv_obj_set_size(bar,
                            run * UI_MENU_ICON_SCALE,
                            UI_MENU_ICON_SCALE);
            lv_obj_set_style_bg_color(bar, UI_COLOR_FG, 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

static void style_menu_label(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, UI_COLOR_FG, 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);

    if (s_menu_font != NULL) {
        lv_obj_set_style_text_font(label, s_menu_font, 0);
    }
}

/*
 * Draw the label twice with a one-pixel horizontal offset.
 * This gives the stock LVGL font a subtle pixel-bold appearance without
 * requiring another font asset.
 */
static void create_bold_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *holder = lv_obj_create(parent);
    lv_obj_remove_style_all(holder);
    lv_obj_set_size(holder, UI_MENU_ITEM_W, 24);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label_a = lv_label_create(holder);
    lv_label_set_text(label_a, text);
    style_menu_label(label_a);
    lv_obj_align(label_a, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *label_b = lv_label_create(holder);
    lv_label_set_text(label_b, text);
    style_menu_label(label_b);
    lv_obj_align(label_b, LV_ALIGN_CENTER, 1, 0);
}

static lv_obj_t *create_menu_item(lv_obj_t *parent, const ui_menu_item_t *item)
{
    lv_obj_t *tile = lv_obj_create(parent);
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, UI_MENU_ITEM_W, UI_MENU_ITEM_H);
    lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tile, 10, 0);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    draw_pixel_bitmap(tile, item->bitmap);
    create_bold_label(tile, item->label);

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
    lv_obj_set_style_pad_top(scroller, 18, 0);
    lv_obj_set_style_pad_bottom(scroller, 18, 0);
    lv_obj_set_style_pad_column(scroller, UI_MENU_GAP, 0);
    lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scroller,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_set_scroll_dir(scroller, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(scroller, LV_SCROLL_SNAP_NONE);

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
