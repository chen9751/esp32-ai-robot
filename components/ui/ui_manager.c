#include "ui_manager.h"
#include "lvgl.h"
#include <stdio.h>

#define W 320
#define H 172
#define BG 0x080B10
#define CARD 0x151A22
#define CARD2 0x202734
#define BLUE 0x3B82F6
#define AMBER 0xF59E0B
#define TEXT 0xF4F7FB
#define MUTED 0x9AA6B2

static lv_obj_t *s_home, *s_apps, *s_settings, *s_light, *s_remote, *s_music, *s_devices;
static lv_obj_t *previous_screen;
static int light_index = 0;
static const char *lights[] = {"Living", "Desk", "Bedroom"};

static void base(lv_obj_t *s) {
    lv_obj_set_style_bg_color(s, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(TEXT), 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
}

static lv_obj_t *label(lv_obj_t *p, const char *txt, int x, int y, int size) {
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, txt);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_text_color(o, lv_color_hex(TEXT), 0);
    if(size >= 28) lv_obj_set_style_text_font(o, &lv_font_montserrat_28, 0);
    else if(size >= 20) lv_obj_set_style_text_font(o, &lv_font_montserrat_20, 0);
    else if(size >= 16) lv_obj_set_style_text_font(o, &lv_font_montserrat_16, 0);
    else lv_obj_set_style_text_font(o, &lv_font_montserrat_14, 0);
    return o;
}

static lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 14, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(CARD), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0x2A3340), 0);
    lv_obj_set_style_pad_all(o, 8, 0);
    return o;
}

static void load(lv_obj_t *s) {
    if(lv_screen_active() != s) previous_screen = lv_screen_active();
    lv_screen_load_anim(s, LV_SCR_LOAD_ANIM_FADE_IN, 160, 0, false);
}

static void go_apps(lv_event_t *e){ (void)e; load(s_apps); }
static void go_light(lv_event_t *e){ (void)e; load(s_light); }
static void go_remote(lv_event_t *e){ (void)e; load(s_remote); }
static void go_music(lv_event_t *e){ (void)e; load(s_music); }
static void go_devices(lv_event_t *e){ (void)e; load(s_devices); }
static void go_back(lv_event_t *e){ (void)e; load(s_apps); }

static void gesture(lv_event_t *e) {
    lv_obj_t *screen = lv_event_get_target(e);
    lv_indev_t *in = lv_indev_active();
    if(!in) return;
    lv_dir_t d = lv_indev_get_gesture_dir(in);
    lv_point_t p; lv_indev_get_point(in, &p);
    if(d == LV_DIR_BOTTOM && p.y < 45 && screen != s_settings) { previous_screen = screen; load(s_settings); return; }
    if(screen == s_home && (d == LV_DIR_LEFT || d == LV_DIR_RIGHT)) { load(s_apps); return; }
    if(screen != s_home && screen != s_apps && screen != s_settings && d == LV_DIR_RIGHT) { load(s_apps); return; }
    if(screen == s_settings && d == LV_DIR_TOP) { load(previous_screen ? previous_screen : s_home); }
}

static void add_gestures(lv_obj_t *s){ lv_obj_add_event_cb(s, gesture, LV_EVENT_GESTURE, NULL); }

static void home_create(void) {
    s_home = lv_obj_create(NULL); base(s_home); add_gestures(s_home);
    lv_obj_t *w = card(s_home, 8, 10, 92, 152);
    label(w, LV_SYMBOL_CLOUDY, 8, 6, 20);
    label(w, "26 C", 8, 35, 28);
    label(w, "Cloudy", 8, 72, 14);
    lv_obj_t *m=label(w, "68%  AQI 32", 8, 108, 14); lv_obj_set_style_text_color(m,lv_color_hex(MUTED),0);
    label(s_home, "SEP 24  THU", 118, 27, 16);
    label(s_home, "12:53", 116, 70, 28);
    lv_obj_t *hint=label(s_home, "Swipe  >  Apps", 180, 142, 14); lv_obj_set_style_text_color(hint,lv_color_hex(MUTED),0);
}

static void app_btn(lv_obj_t *p, const char *icon, const char *txt, int x, lv_event_cb_t cb) {
    lv_obj_t *b=card(p,x,35,70,100); lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *i=label(b,icon,18,12,28); lv_obj_set_style_text_color(i,lv_color_hex(BLUE),0);
    label(b,txt,6,62,14);
}

static void apps_create(void) {
    s_apps=lv_obj_create(NULL); base(s_apps); add_gestures(s_apps);
    label(s_apps,"HOME",10,8,14); label(s_apps,"Functions",118,8,16);
    app_btn(s_apps,LV_SYMBOL_LIST,"Remote",10,go_remote);
    app_btn(s_apps,LV_SYMBOL_AUDIO,"Music",88,go_music);
    app_btn(s_apps,LV_SYMBOL_EYE_OPEN,"Lights",166,go_light);
    app_btn(s_apps,LV_SYMBOL_HOME,"Devices",244,go_devices);
}

static void slider_row(lv_obj_t *p,const char *name,int y,int value){
    label(p,name,14,y,14);
    lv_obj_t *s=lv_slider_create(p); lv_obj_set_pos(s,100,y+2); lv_obj_set_size(s,190,10);
    lv_slider_set_value(s,value,LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s,lv_color_hex(BLUE),LV_PART_INDICATOR);
}

static void settings_create(void) {
    s_settings=lv_obj_create(NULL); base(s_settings); add_gestures(s_settings);
    label(s_settings,"System settings",12,10,20);
    slider_row(s_settings,"Volume",52,60);
    slider_row(s_settings,"Brightness",96,80);
    lv_obj_t *h=label(s_settings,"Swipe up to close",96,145,14); lv_obj_set_style_text_color(h,lv_color_hex(MUTED),0);
}

static void light_create(void) {
    s_light=lv_obj_create(NULL); base(s_light); add_gestures(s_light);
    label(s_light,"<",8,8,20); label(s_light,lights[light_index],38,10,20);
    lv_obj_t *sw=lv_switch_create(s_light); lv_obj_set_pos(sw,260,10); lv_obj_add_state(sw,LV_STATE_CHECKED);
    slider_row(s_light,"Brightness",55,72);
    slider_row(s_light,"Warm / Cool",91,58);
    const char *p[]={"Relax","Read","Movie","Night"};
    for(int i=0;i<4;i++){lv_obj_t *b=card(s_light,8+i*78,128,70,36); label(b,p[i],4,2,14);}
}

static void remote_create(void) {
    s_remote=lv_obj_create(NULL); base(s_remote); add_gestures(s_remote);
    label(s_remote,"TV Remote",108,7,16);
    lv_obj_t *pad=card(s_remote,84,31,152,82);
    label(pad,LV_SYMBOL_UP,62,0,20); label(pad,LV_SYMBOL_DOWN,62,48,20);
    label(pad,LV_SYMBOL_LEFT,12,24,20); label(pad,LV_SYMBOL_RIGHT,112,24,20);
    lv_obj_t *ok=lv_btn_create(pad); lv_obj_set_size(ok,44,44); lv_obj_align(ok,LV_ALIGN_CENTER,0,0);
    label(ok,"OK",8,4,14);
    lv_obj_t *back=card(s_remote,12,125,66,36); label(back,"Back",8,2,14);
    lv_obj_t *home=card(s_remote,88,125,66,36); label(home,LV_SYMBOL_HOME,16,2,20);
    lv_obj_t *play=card(s_remote,164,125,66,36); label(play,LV_SYMBOL_PLAY,16,2,20);
    lv_obj_t *power=card(s_remote,240,125,66,36); label(power,LV_SYMBOL_POWER,16,2,20);
}

static void music_create(void) {
    s_music=lv_obj_create(NULL); base(s_music); add_gestures(s_music);
    lv_obj_t *cover=card(s_music,10,14,104,104); label(cover,LV_SYMBOL_AUDIO,30,25,28);
    label(s_music,"Time Traveler",130,20,20);
    lv_obj_t *a=label(s_music,"Zhou Shen",130,48,14); lv_obj_set_style_text_color(a,lv_color_hex(MUTED),0);
    lv_obj_t *bar=lv_slider_create(s_music); lv_obj_set_pos(bar,130,78); lv_obj_set_size(bar,176,8); lv_slider_set_value(bar,36,LV_ANIM_OFF);
    label(s_music,"01:24                         04:26",130,91,14);
    label(s_music,LV_SYMBOL_PREV,150,127,20);
    lv_obj_t *pb=lv_btn_create(s_music); lv_obj_set_pos(pb,194,116); lv_obj_set_size(pb,48,48); label(pb,LV_SYMBOL_PAUSE,10,6,20);
    label(s_music,LV_SYMBOL_NEXT,266,127,20);
}

static void devices_create(void) {
    s_devices=lv_obj_create(NULL); base(s_devices); add_gestures(s_devices);
    label(s_devices,"Devices",12,10,20);
    label(s_devices,LV_SYMBOL_HOME,144,48,28);
    label(s_devices,"No devices yet",106,92,16);
    lv_obj_t *t=label(s_devices,"Reserved for HA / robot devices",58,124,14); lv_obj_set_style_text_color(t,lv_color_hex(MUTED),0);
}

void ui_init(void) {
    home_create(); apps_create(); settings_create(); light_create(); remote_create(); music_create(); devices_create();
    previous_screen=s_home;
    lv_screen_load(s_home);
}

void ui_show_home(void){ load(s_home); }
void ui_show_functions(void){ load(s_apps); }
void ui_show_settings(void){ load(s_settings); }
