#include "ui_system_icons.h"

#define ICON_SCALE 340

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *bar(lv_obj_t *parent, int32_t w, int32_t h, lv_color_t color)
{
    lv_obj_t *obj = plain(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    return obj;
}

static lv_obj_t *symbol_icon(lv_obj_t *parent, const char *symbol, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_transform_scale_x(label, ICON_SCALE, 0);
    lv_obj_set_style_transform_scale_y(label, ICON_SCALE, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

lv_obj_t *ui_system_icon_wifi(lv_obj_t *parent, lv_color_t color)
{
    return symbol_icon(parent, LV_SYMBOL_WIFI, color);
}

lv_obj_t *ui_system_icon_volume(lv_obj_t *parent, lv_color_t color)
{
    return symbol_icon(parent, LV_SYMBOL_VOLUME_MAX, color);
}

lv_obj_t *ui_system_icon_brightness(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *root = plain(parent);
    lv_obj_set_size(root, 36, 36);

    lv_obj_t *core = plain(root);
    lv_obj_set_size(core, 16, 16);
    lv_obj_align(core, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(core, 2, 0);
    lv_obj_set_style_border_color(core, color, 0);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(core, LV_OBJ_FLAG_CLIP_CHILDREN);

    lv_obj_t *half = plain(core);
    lv_obj_set_size(half, 8, 16);
    lv_obj_set_pos(half, 8, 0);
    lv_obj_set_style_bg_color(half, color, 0);
    lv_obj_set_style_bg_opa(half, LV_OPA_COVER, 0);

    lv_obj_t *top = bar(root, 2, 5, color);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 1);
    lv_obj_t *bottom = bar(root, 2, 5, color);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, -1);
    lv_obj_t *left = bar(root, 5, 2, color);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 1, 0);
    lv_obj_t *right = bar(root, 5, 2, color);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -1, 0);

    lv_obj_t *tl = bar(root, 5, 2, color);
    lv_obj_set_pos(tl, 5, 5);
    lv_obj_set_style_transform_rotation(tl, 450, 0);
    lv_obj_t *tr = bar(root, 5, 2, color);
    lv_obj_set_pos(tr, 26, 5);
    lv_obj_set_style_transform_rotation(tr, 1350, 0);
    lv_obj_t *bl = bar(root, 5, 2, color);
    lv_obj_set_pos(bl, 5, 29);
    lv_obj_set_style_transform_rotation(bl, 1350, 0);
    lv_obj_t *br = bar(root, 5, 2, color);
    lv_obj_set_pos(br, 26, 29);
    lv_obj_set_style_transform_rotation(br, 450, 0);

    return root;
}

static void recolor_tree(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_border_color(obj, color, 0);
    lv_obj_set_style_arc_color(obj, color, LV_PART_MAIN);

    uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; ++i) {
        recolor_tree(lv_obj_get_child(obj, i), color);
    }
}

void ui_system_icon_set_color(lv_obj_t *icon, lv_color_t color)
{
    if (icon != NULL) recolor_tree(icon, color);
}
