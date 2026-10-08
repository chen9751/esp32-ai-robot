#include "ui_page_home.h"
#include "ui_assets.h"

#include <stddef.h>
#include <stdint.h>

#define UI_SCREEN_W          640
#define UI_SCREEN_H          172
#define UI_MENU_ITEM_W       136
#define UI_MENU_ITEM_H       136
#define UI_MENU_GAP          20
#define UI_MENU_SIDE_PAD     18
#define UI_MENU_V_PAD        18
#define UI_MENU_RADIUS       30
#define UI_MENU_PRESS_OFFSET_Y  2
#define UI_MENU_PRESS_MS      55
#define UI_MENU_RELEASE_MS    70

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
static ui_page_activity_cb_t s_activity_cb = NULL;
static void *s_activity_user_data = NULL;
static ui_vertical_drag_cb_t s_vertical_drag_cb = NULL;
static void *s_vertical_drag_user_data = NULL;
static lv_point_t s_drag_press = {0, 0};
static bool s_drag_valid = false;
static bool s_drag_was_vertical = false;
static bool s_drag_swipe_seen = false;

static const ui_menu_item_t MENU_ITEMS[] = {
    { &ui_icon_remote,   &ui_label_remote,   UI_MENU_REMOTE,   0x31B9FFu, 0x075EF0u },
    { &ui_icon_music,    &ui_label_music,    UI_MENU_MUSIC,    0xFF687Eu, 0xC92D70u },
    { &ui_icon_light,    &ui_label_light,    UI_MENU_LIGHTS,   0xFFD66Au, 0xFF913Eu },
    { &ui_icon_devices,  &ui_label_devices,  UI_MENU_DEVICES,  0x48D8B1u, 0x0AA57Fu },
};

static void note_activity(void)
{
    if (s_activity_cb != NULL) {
        s_activity_cb(s_activity_user_data);
    }
}

static void tile_translate_y_set(void *obj, int32_t value)
{
    lv_obj_set_style_translate_y((lv_obj_t *)obj, value, 0);
}

static void animate_tile_translate_y(lv_obj_t *tile, int32_t end_y, uint32_t duration_ms)
{
    if (tile == NULL) {
        return;
    }

    lv_anim_delete(tile, tile_translate_y_set);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, tile);
    lv_anim_set_values(&anim,
                       lv_obj_get_style_translate_y(tile, LV_PART_MAIN),
                       end_y);
    lv_anim_set_duration(&anim, duration_ms);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, tile_translate_y_set);
    lv_anim_start(&anim);
}

static void menu_item_pressed(lv_event_t *e)
{
    lv_obj_t *tile = lv_event_get_current_target(e);
    note_activity();
    animate_tile_translate_y(tile, UI_MENU_PRESS_OFFSET_Y, UI_MENU_PRESS_MS);
}

static void menu_item_released(lv_event_t *e)
{
    lv_obj_t *tile = lv_event_get_current_target(e);
    animate_tile_translate_y(tile, 0, UI_MENU_RELEASE_MS);
}

static void menu_item_clicked(lv_event_t *e)
{
    const ui_menu_item_t *item = (const ui_menu_item_t *)lv_event_get_user_data(e);
    note_activity();

    if (s_drag_was_vertical || ui_navigation_transition_active()) {
        return;
    }

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
    lv_obj_set_style_translate_y(tile, 0, 0);

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
    lv_obj_add_flag(tile, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    create_icon(tile, item->icon);
    create_menu_label(tile, item->label_image);

    lv_obj_add_event_cb(tile, menu_item_pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(tile, menu_item_released, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(tile, menu_item_released, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(tile, menu_item_clicked, LV_EVENT_CLICKED, (void *)item);
    return tile;
}

static void home_drag_event_cb(lv_event_t *e)
{
    if (s_vertical_drag_cb == NULL) {
        return;
    }

    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_drag_press);
        s_drag_valid = true;
        s_drag_was_vertical = false;
        s_drag_swipe_seen = false;
        note_activity();
        return;
    }

    if ((code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) &&
        s_drag_valid) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);

        int32_t dx = point.x - s_drag_press.x;
        int32_t dy = point.y - s_drag_press.y;
        if (dy >= 12 || dy <= -12) {
            if ((dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx))
                s_drag_was_vertical = true;
        }
        if (s_drag_was_vertical) {
            if (!s_drag_swipe_seen) {
                s_drag_swipe_seen = true;
                /* Clear any pressed visual on the tile when the user swipes. */
                lv_obj_t *target = lv_event_get_target(e);
                if (target && target != lv_event_get_current_target(e))
                    animate_tile_translate_y(target, 0, UI_MENU_RELEASE_MS);
            }
            s_vertical_drag_cb(dx, dy, code == LV_EVENT_RELEASED,
                               false, s_vertical_drag_user_data);
        }

        if (code == LV_EVENT_RELEASED) {
            s_drag_valid = false;
        }
        return;
    }

    if (code == LV_EVENT_PRESS_LOST && s_drag_valid) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);

        int32_t dx = point.x - s_drag_press.x;
        int32_t dy = point.y - s_drag_press.y;
        if (s_drag_was_vertical ||
            ((dy >= 12 || dy <= -12) &&
             (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx))) {
            s_drag_was_vertical = true;
            s_vertical_drag_cb(dx, dy, true, true, s_vertical_drag_user_data);
        }
        s_drag_valid = false;
    }
}

lv_obj_t *ui_page_home_build(lv_obj_t *parent,
                             ui_menu_action_cb_t action_cb,
                             void *action_user_data,
                             ui_page_activity_cb_t activity_cb,
                             void *activity_user_data,
                             ui_vertical_drag_cb_t vertical_drag_cb,
                             void *vertical_drag_user_data)
{
    s_action_cb = action_cb;
    s_action_user_data = action_user_data;
    s_activity_cb = activity_cb;
    s_activity_user_data = activity_user_data;
    s_vertical_drag_cb = vertical_drag_cb;
    s_vertical_drag_user_data = vertical_drag_user_data;
    s_drag_valid = false;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *scroller = lv_obj_create(root);
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

    /* Exactly four fixed tiles: 4*136 + 3*20 + 2*18 = 640. */
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(scroller, LV_SCROLL_SNAP_NONE);
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_clear_flag(scroller, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_obj_add_event_cb(scroller, home_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scroller, home_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(scroller, home_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(scroller, home_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);

    for (size_t i = 0; i < sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]); ++i) {
        create_menu_item(scroller, &MENU_ITEMS[i]);
    }

    return root;
}

void ui_page_home_show(ui_menu_action_cb_t action_cb,
                       void *action_user_data,
                       ui_page_activity_cb_t activity_cb,
                       void *activity_user_data,
                       ui_vertical_drag_cb_t vertical_drag_cb,
                       void *vertical_drag_user_data)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);

    lv_obj_set_size(screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(screen, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    ui_page_home_build(screen,
                       action_cb,
                       action_user_data,
                       activity_cb,
                       activity_user_data,
                       vertical_drag_cb,
                       vertical_drag_user_data);
}
