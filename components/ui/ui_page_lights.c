#include "ui_page_lights.h"
#include "ui_lights_labels.h"

#include <stddef.h>
#include <stdint.h>

#define UI_SCREEN_W 640
#define UI_SCREEN_H 172
#define ITEM_W 142
#define ITEM_H 172
#define LEFT_PAD 58
#define RIGHT_PAD 18
#define GAP 2
#define CTRL_Y 134
#define CTRL_H 30

#define C_BG       lv_color_hex(0x000000)
#define C_TEXT     lv_color_hex(0xF5F5F7)
#define C_OFF      lv_color_hex(0x777B82)
#define C_OFF_DARK lv_color_hex(0x2A2D31)
#define C_DIV      lv_color_hex(0x222428)
#define C_WARM     lv_color_hex(0xFFD37A)
#define C_WARM_HI  lv_color_hex(0xFFF1C4)
#define C_CTRL_BG  lv_color_hex(0x111214)

typedef enum { LIGHT_NORMAL=0, LIGHT_RGB, LIGHT_SWITCH_ONLY } light_kind_t;
typedef enum { ICON_SOFA=0, ICON_PC, ICON_BED, ICON_BEDSIDE, ICON_BUNK, ICON_TV, ICON_BATH, ICON_BALCONY } room_icon_t;

typedef struct {
    const lv_image_dsc_t *label;
    room_icon_t icon_type;
    light_kind_t kind;
    bool initial_on;
} light_spec_t;

typedef struct {
    lv_obj_t *item;
    lv_obj_t *lamp_bar;
    lv_obj_t *beam[12];
    lv_obj_t *icon_holder;
    lv_obj_t *controls[3];
    light_kind_t kind;
    bool on;
} light_view_t;

static const light_spec_t SPECS[8] = {
    { &ui_lights_label_living,        ICON_SOFA,    LIGHT_NORMAL,      true  },
    { &ui_lights_label_study,         ICON_PC,      LIGHT_NORMAL,      false },
    { &ui_lights_label_bedroom,       ICON_BED,     LIGHT_NORMAL,      true  },
    { &ui_lights_label_bedside,       ICON_BEDSIDE, LIGHT_RGB,         true  },
    { &ui_lights_label_small_bedroom, ICON_BUNK,    LIGHT_NORMAL,      false },
    { &ui_lights_label_rgb_strip,     ICON_TV,      LIGHT_RGB,         true  },
    { &ui_lights_label_bathroom,      ICON_BATH,    LIGHT_SWITCH_ONLY, false },
    { &ui_lights_label_balcony,       ICON_BALCONY, LIGHT_SWITCH_ONLY, true  },
};

static lv_obj_t *s_root;
static ui_lights_activity_cb_t s_activity_cb;
static void *s_activity_user_data;
static light_view_t s_views[8];
static bool s_labels_ready;

static void note_activity(void) { if (s_activity_cb) s_activity_cb(s_activity_user_data); }

static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, int r)
{
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_style_all(o); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,C_TEXT,0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); lv_obj_set_style_radius(o,r,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE); return o;
}

static void tint_children(lv_obj_t *p, lv_color_t c, lv_opa_t opa)
{
    uint32_t n=lv_obj_get_child_count(p);
    for(uint32_t i=0;i<n;i++){ lv_obj_t *ch=lv_obj_get_child(p,i); lv_obj_set_style_bg_color(ch,c,0); lv_obj_set_style_bg_opa(ch,opa,0); }
}

static lv_obj_t *create_room_icon(lv_obj_t *p, room_icon_t t)
{
    lv_obj_t *h=lv_obj_create(p); lv_obj_remove_style_all(h); lv_obj_set_size(h,54,54); lv_obj_set_pos(h,44,68); lv_obj_clear_flag(h,LV_OBJ_FLAG_SCROLLABLE);
    switch(t){
        case ICON_SOFA:
            box(h,10,24,34,16,6); box(h,14,16,26,15,6); box(h,6,28,6,14,3); box(h,42,28,6,14,3); box(h,12,40,4,7,2); box(h,38,40,4,7,2); break;
        case ICON_PC:
            box(h,7,10,40,27,3); box(h,11,14,32,19,1); lv_obj_set_style_bg_color(lv_obj_get_child(h,1),C_BG,0); box(h,25,37,4,6,1); box(h,17,43,20,4,2); break;
        case ICON_BED:
            box(h,8,28,39,13,3); box(h,8,17,5,29,2); box(h,14,22,14,8,4); box(h,29,22,14,8,4); box(h,13,38,34,4,2); break;
        case ICON_BEDSIDE:
            box(h,15,29,24,17,4); box(h,24,18,6,11,2); box(h,18,10,18,10,4); box(h,23,35,8,4,2); break;
        case ICON_BUNK:
            box(h,10,8,5,39,2); box(h,39,8,5,39,2); box(h,14,18,26,5,2); box(h,14,35,26,5,2); box(h,17,12,17,6,3); box(h,17,29,17,6,3); break;
        case ICON_TV:
            box(h,7,11,40,27,4); box(h,11,15,32,19,2); lv_obj_set_style_bg_color(lv_obj_get_child(h,1),C_BG,0); box(h,25,38,4,6,1); box(h,17,44,20,4,2); break;
        case ICON_BATH:
            box(h,7,27,40,15,7); box(h,7,24,40,6,3); box(h,35,12,4,14,2); box(h,35,11,10,4,2); box(h,10,41,4,6,2); box(h,40,41,4,6,2); break;
        case ICON_BALCONY:
            box(h,7,10,4,38,2); box(h,43,10,4,38,2); box(h,7,14,40,4,2); box(h,18,20,4,20,2); box(h,31,20,4,20,2); box(h,14,20,12,5,2); box(h,28,20,12,5,2); break;
    }
    return h;
}

static void create_beam(light_view_t *v)
{
    for(int i=0;i<12;i++){
        int w=44+i*6; int x=(ITEM_W-w)/2; int y=35+i*7;
        lv_obj_t *b=box(v->item,x,y,w,8,2); v->beam[i]=b; lv_obj_move_background(b);
    }
}

static lv_obj_t *create_small_icon(lv_obj_t *p, int kind)
{
    lv_obj_t *h=lv_obj_create(p); lv_obj_remove_style_all(h); lv_obj_set_size(h,24,24); lv_obj_center(h); lv_obj_clear_flag(h,LV_OBJ_FLAG_SCROLLABLE);
    if(kind==0){ box(h,8,8,8,8,4); box(h,11,1,2,5,1); box(h,11,18,2,5,1); box(h,1,11,5,2,1); box(h,18,11,5,2,1); }
    else if(kind==1){ box(h,10,3,4,13,2); box(h,7,14,10,10,5); box(h,11,7,2,12,1); }
    else { box(h,3,8,18,3,2); box(h,5,12,14,3,2); box(h,8,16,8,3,2); }
    return h;
}

static lv_obj_t *make_control(lv_obj_t *p, int x, int kind)
{
    lv_obj_t *b=lv_obj_create(p); lv_obj_remove_style_all(b); lv_obj_set_pos(b,x,CTRL_Y); lv_obj_set_size(b,36,CTRL_H);
    lv_obj_set_style_bg_color(b,C_CTRL_BG,0); lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0); lv_obj_set_style_border_width(b,1,0); lv_obj_set_style_radius(b,10,0);
    lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(b,LV_OBJ_FLAG_SCROLLABLE); create_small_icon(b,kind); return b;
}

static void update_state(light_view_t *v)
{
    lv_color_t main=v->on?C_WARM_HI:C_OFF; lv_color_t beam=v->on?C_WARM:C_OFF_DARK;
    lv_obj_set_style_bg_color(v->lamp_bar,main,0); lv_obj_set_style_bg_opa(v->lamp_bar,v->on?LV_OPA_COVER:LV_OPA_60,0);
    lv_obj_set_style_shadow_color(v->lamp_bar,v->on?C_WARM:C_OFF_DARK,0); lv_obj_set_style_shadow_width(v->lamp_bar,v->on?18:4,0); lv_obj_set_style_shadow_opa(v->lamp_bar,v->on?LV_OPA_60:LV_OPA_20,0);
    for(int i=0;i<12;i++){
        lv_color_t c=beam; if(v->kind==LIGHT_RGB && v->on){ if(i<4)c=lv_color_hex(0xFF5C73); else if(i<8)c=lv_color_hex(0x63E6BE); else c=lv_color_hex(0x74C0FC); }
        lv_obj_set_style_bg_color(v->beam[i],c,0); lv_obj_set_style_bg_opa(v->beam[i],v->on?(lv_opa_t)(125-i*7):(lv_opa_t)(55-i*3),0);
    }
    tint_children(v->icon_holder,main,v->on?LV_OPA_COVER:LV_OPA_70);
    for(int i=0;i<3;i++) if(v->controls[i]){ lv_obj_set_style_border_color(v->controls[i],v->on?lv_color_hex(0x4A3A24):C_OFF_DARK,0); lv_obj_t *ic=lv_obj_get_child(v->controls[i],0); if(ic)tint_children(ic,main,v->on?LV_OPA_COVER:LV_OPA_70); }
}

static void item_click(lv_event_t *e){ light_view_t *v=(light_view_t*)lv_event_get_user_data(e); if(v){v->on=!v->on;update_state(v);note_activity();} }
static void scroll_activity(lv_event_t *e){(void)e;note_activity();}

static void create_switch_footer(lv_obj_t *p)
{
    lv_obj_t *b=box(p,25,CTRL_Y,92,CTRL_H,14); lv_obj_set_style_bg_color(b,C_CTRL_BG,0); lv_obj_set_style_bg_opa(b,LV_OPA_70,0); box(b,31,14,30,3,2); lv_obj_set_style_bg_color(lv_obj_get_child(b,0),C_OFF_DARK,0);
}

static void create_item(lv_obj_t *parent,size_t idx)
{
    const light_spec_t *s=&SPECS[idx]; light_view_t *v=&s_views[idx]; *v=(light_view_t){0}; v->kind=s->kind; v->on=s->initial_on;
    v->item=lv_obj_create(parent); lv_obj_remove_style_all(v->item); lv_obj_set_size(v->item,ITEM_W,ITEM_H); lv_obj_set_style_bg_color(v->item,C_BG,0); lv_obj_set_style_bg_opa(v->item,LV_OPA_COVER,0); lv_obj_clear_flag(v->item,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(v->item,LV_OBJ_FLAG_CLICKABLE); lv_obj_add_flag(v->item,LV_OBJ_FLAG_EVENT_BUBBLE); lv_obj_add_event_cb(v->item,item_click,LV_EVENT_CLICKED,v);
    lv_obj_t *label=lv_image_create(v->item); lv_image_set_src(label,s->label); lv_obj_set_style_image_recolor(label,C_TEXT,0); lv_obj_set_style_image_recolor_opa(label,LV_OPA_COVER,0); lv_obj_align(label,LV_ALIGN_TOP_MID,0,3); lv_obj_clear_flag(label,LV_OBJ_FLAG_CLICKABLE);
    v->lamp_bar=box(v->item,36,28,70,8,4); create_beam(v); v->icon_holder=create_room_icon(v->item,s->icon_type);
    if(s->kind==LIGHT_SWITCH_ONLY) create_switch_footer(v->item); else if(s->kind==LIGHT_NORMAL){v->controls[0]=make_control(v->item,31,0);v->controls[1]=make_control(v->item,75,1);} else {v->controls[0]=make_control(v->item,10,0);v->controls[1]=make_control(v->item,53,1);v->controls[2]=make_control(v->item,96,2);}
    if(idx!=7){lv_obj_t *d=box(v->item,ITEM_W-1,20,1,132,0);lv_obj_set_style_bg_color(d,C_DIV,0);}
    update_state(v);
}

lv_obj_t *ui_page_lights_build(lv_obj_t *parent, ui_lights_activity_cb_t activity_cb, void *activity_user_data)
{
    s_activity_cb=activity_cb; s_activity_user_data=activity_user_data; if(!s_labels_ready){ui_lights_labels_init();s_labels_ready=true;}
    s_root=lv_obj_create(parent); lv_obj_remove_style_all(s_root); lv_obj_set_size(s_root,UI_SCREEN_W,UI_SCREEN_H); lv_obj_set_style_bg_color(s_root,C_BG,0); lv_obj_set_style_bg_opa(s_root,LV_OPA_COVER,0); lv_obj_clear_flag(s_root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sc=lv_obj_create(s_root); lv_obj_remove_style_all(sc); lv_obj_set_size(sc,UI_SCREEN_W,UI_SCREEN_H); lv_obj_set_style_bg_color(sc,C_BG,0); lv_obj_set_style_bg_opa(sc,LV_OPA_COVER,0); lv_obj_set_style_pad_left(sc,LEFT_PAD,0); lv_obj_set_style_pad_right(sc,RIGHT_PAD,0); lv_obj_set_style_pad_column(sc,GAP,0); lv_obj_set_flex_flow(sc,LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(sc,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER); lv_obj_set_scroll_dir(sc,LV_DIR_HOR); lv_obj_set_scrollbar_mode(sc,LV_SCROLLBAR_MODE_OFF); lv_obj_clear_flag(sc,LV_OBJ_FLAG_SCROLL_ONE); lv_obj_clear_flag(sc,LV_OBJ_FLAG_SCROLL_ELASTIC); lv_obj_add_event_cb(sc,scroll_activity,LV_EVENT_SCROLL_BEGIN,NULL); lv_obj_add_event_cb(sc,scroll_activity,LV_EVENT_SCROLL,NULL);
    for(size_t i=0;i<8;i++)create_item(sc,i); return s_root;
}

void ui_page_lights_stop(void){ if(s_root)lv_obj_delete(s_root); s_root=NULL; s_activity_cb=NULL; s_activity_user_data=NULL; }
