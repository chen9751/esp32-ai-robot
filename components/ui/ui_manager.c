#include "ui_manager.h"

#include <stddef.h>
#include <stdint.h>

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172
#define UI_MENU_ITEM_W       144
#define UI_MENU_ITEM_H       140
#define UI_MENU_ICON_SCALE   5
#define UI_MENU_ICON_SIZE    (16 * UI_MENU_ICON_SCALE)
#define UI_MENU_GAP          28
#define UI_MENU_SIDE_PAD     28

/* Mint/cyan green chosen to sit close to the Waveshare blue-green enclosure. */
#define UI_COLOR_BG          lv_color_hex(0x050909)
#define UI_COLOR_FG          lv_color_hex(0x69D8C8)
#define UI_COLOR_FG_DIM      lv_color_hex(0x315F59)

typedef struct {
    const char *label;
    ui_menu_action_t action;
    const uint16_t *bitmap;
} ui_menu_item_t;

static ui_menu_action_cb_t s_action_cb = NULL;
static void *s_action_user_data = NULL;
static const lv_font_t *s_menu_font = NULL;

/*
 * 16x16 monochrome bitmaps.
 * Bit 15 is the left-most pixel. They are rendered as hard square pixels,
 * deliberately avoiding anti-aliased vector icons.
 */
static const uint16_t ICON_REMOTE[16] = {
    0x0000, 0x0180, 0x03C0, 0x0660,
    0x0000, 0x07E0, 0x0420, 0x05A0,
    0x05A0, 0x0420, 0x05A0, 0x0420,
    0x05A0, 0x0420, 0x07E0, 0x0000
};

static const uint16_t ICON_MUSIC[16] = {
    0x0000, 0x0780, 0x0480, 0x0480,
    0x0480, 0x04F0, 0x0490, 0x0490,
    0x0490, 0x0D90, 0x1B10, 0x1210,
    0x0C10, 0x0010, 0x0030, 0x0000
};

static const uint16_t ICON_LIGHT[16] = {
    0x0180, 0x0180, 0x2184, 0x1188,
    0x07E0, 0x0FF0, 0x1818, 0x1818,
    0x0C30, 0x0660, 0x03C0, 0x0180,
    0x03C0, 0x03C0, 0x0180, 0x0000
};

static const uint16_t ICON_DEVICE[16] = {
    0x0000, 0x0180, 0x03C0, 0x07E0,
    0x0FF0, 0x1FF8, 0x3FFC, 0x6186,
    0x4182, 0x4182, 0x41E2, 0x41E2,
    0x4182, 0x7FFE, 0x0000, 0x0000
};

static const uint16_t ICON_ALARM[16] = {
    0x0660, 0x0990, 0x0000, 0x07E0,
    0x1818, 0x300C, 0x6186, 0x6186,
    0x61E6, 0x6006, 0x300C, 0x1818,
    0x07E0, 0x0660, 0x0C30, 0x0000
};

static const uint16_t ICON_SETTINGS[16] = {
    0x0180, 0x0990, 0x0FF0, 0x07E0,
    0x318C, 0x73CE, 0x63C6, 0xC3C3,
    0xC3C3, 0x63C6, 0x73CE, 0x318C,
    0x07E0, 0x0FF0, 0x0990, 0x0180
};

static const ui_menu_item_t MENU_ITEMS[] = {
    { "REMOTE",  UI_MENU_REMOTE,   ICON_REMOTE   },
    { "MUSIC",   UI_MENU_MUSIC,    ICON_MUSIC    },
    { "LIGHTS",  UI_MENU_LIGHTS,   ICON_LIGHT    },
    { "DEVICES", UI_MENU_DEVICES,  ICON_DEVICE   },
    { "ALARM",   UI_MENU_ALARM,    ICON_ALARM    },
    { "SETTINGS",UI_MENU_SETTINGS, ICON_SETTINGS },
};

static void menu_item_clicked(lv_event_t *e)
{
    const ui_menu_item_t *item = (const ui_menu_item_t *)lv_event_get_user_data(e);

    if (item != NULL && s_action_cb != NULL) {
        s_action_cb(item->action, s_action_user_data);
    }
}

static void draw_pixel_bitmap(lv_obj_t *parent, const uint16_t bitmap[16])
{
    lv_obj_t *pixel_layer = lv_obj_create(parent);
    lv_obj_remove_style_all(pixel_layer);
    lv_obj_set_size(pixel_layer, UI_MENU_ICON_SIZE, UI_MENU_ICON_SIZE);
    lv_obj_set_style_bg_opa(pixel_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(pixel_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(pixel_layer, LV_OBJ_FLAG_CLICKABLE);

    for (int y = 0; y < 16; ++y) {
        uint16_t row = bitmap[y];
        int x = 0;

        while (x < 16) {
            while (x < 16 && ((row & (uint16_t)(0x8000u >> x)) == 0)) {
                ++x;
            }
            if (x >= 16) {
                break;
            }

            int start = x;
            while (x < 16 && (row & (uint16_t)(0x8000u >> x))) {
                ++x;
            }

            int run = x - start;
            lv_obj_t *bar = lv_obj_create(pixel_layer);
            lv_obj_remove_style_all(bar);
            lv_obj_set_pos(bar, start * UI_MENU_ICON_SCALE, y * UI_MENU_ICON_SCALE);
            lv_obj_set_size(bar, run * UI_MENU_ICON_SCALE, UI_MENU_ICON_SCALE);
            lv_obj_set_style_bg_color(bar, UI_COLOR_FG, 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        }
    }
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

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, item->label);
    lv_obj_set_style_text_color(label, UI_COLOR_FG, 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    if (s_menu_font != NULL) {
        lv_obj_set_style_text_font(label, s_menu_font, 0);
    }

    lv_obj_add_event_cb(tile, menu_item_clicked, LV_EVENT_CLICKED, (void *)item);
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
    lv_obj_set_style_pad_top(scroller, 16, 0);
    lv_obj_set_style_pad_bottom(scroller, 16, 0);
    lv_obj_set_style_pad_column(scroller, UI_MENU_GAP, 0);
    lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scroller,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_set_scroll_dir(scroller, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(scroller, LV_SCROLL_SNAP_NONE);

    for (size_t i = 0; i < sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]); ++i) {
        create_menu_item(scroller, &MENU_ITEMS[i]);
    }

    /* A small end marker hints that the row can continue horizontally. */
    lv_obj_t *end_mark = lv_obj_create(scroller);
    lv_obj_remove_style_all(end_mark);
    lv_obj_set_size(end_mark, 8, 48);
    lv_obj_set_style_bg_color(end_mark, UI_COLOR_FG_DIM, 0);
    lv_obj_set_style_bg_opa(end_mark, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(end_mark, 0, 0);
    lv_obj_clear_flag(end_mark, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(end_mark, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_init(void)
{
    ui_show_main_menu();
}
