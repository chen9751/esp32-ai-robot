#include "ui_manager.h"
#include "lvgl.h"

#define BG 0x06080D
#define CARD 0x111722
#define CARD2 0x182131
#define BLUE 0x4D8DFF
#define TEXT 0xF7F9FC
#define MUTED 0x8996A8
#define LINE 0x263244
#define HOME_SKY_TOP 0x0878C9
#define HOME_SKY_BOTTOM 0x2E8FD1
#define HOME_SUN 0xFFD34E
#define HOME_GLASS 0x082F57

static lv_obj_t *s_home,*s_apps,*s_settings,*s_light,*s_remote,*s_music,*s_devices;
static lv_obj_t *return_screen,*light_title,*light_switch,*light_bri,*light_temp;
static lv_obj_t *music_play_label,*music_progress,*volume_slider,*brightness_slider;
static int light_index; static bool music_playing=true; static lv_point_t press_start; static lv_obj_t *press_screen;
static const char *lights[]={"Living room","Desk lamp","Bedroom"};

__attribute__((weak)) void ui_action_volume(int v){(void)v;}
__attribute__((weak)) void ui_action_brightness(int v){(void)v;}
__attribute__((weak)) void ui_action_light_select(int v){(void)v;}
__attribute__((weak)) void ui_action_light_power(bool v){(void)v;}
__attribute__((weak)) void ui_action_light_brightness(int v){(void)v;}
__attribute__((weak)) void ui_action_light_temperature(int v){(void)v;}
__attribute__((weak)) void ui_action_light_preset(int v){(void)v;}
__attribute__((weak)) void ui_action_music(ui_music_action_t v){(void)v;}
__attribute__((weak)) void ui_action_music_seek(int v){(void)v;}
__attribute__((weak)) void ui_action_remote(ui_remote_action_t v){(void)v;}
__attribute__((weak)) void ui_action_orientation(bool portrait){(void)portrait;}

static void base(lv_obj_t*s){lv_obj_set_style_bg_color(s,lv_color_hex(BG),0);lv_obj_set_style_bg_opa(s,LV_OPA_COVER,0);lv_obj_set_style_text_color(s,lv_color_hex(TEXT),0);lv_obj_set_style_border_width(s,0,0);lv_obj_set_style_pad_all(s,0,0);}
static lv_obj_t *label(lv_obj_t*p,const char*t,int x,int y,int z){lv_obj_t*o=lv_label_create(p);lv_label_set_text(o,t);lv_obj_set_pos(o,x,y);lv_obj_set_style_text_color(o,lv_color_hex(TEXT),0);lv_obj_set_style_text_font(o,z>=28?&lv_font_montserrat_28:z>=20?&lv_font_montserrat_20:z>=16?&lv_font_montserrat_16:&lv_font_montserrat_14,0);return o;}
static lv_obj_t *card(lv_obj_t*p,int x,int y,int w,int h){lv_obj_t*o=lv_obj_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_radius(o,18,0);lv_obj_set_style_bg_color(o,lv_color_hex(CARD),0);lv_obj_set_style_bg_grad_color(o,lv_color_hex(CARD2),0);lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_HOR,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(LINE),0);lv_obj_set_style_pad_all(o,10,0);return o;}
static void load(lv_obj_t*s){lv_screen_load_anim(s,LV_SCR_LOAD_ANIM_FADE_IN,120,0,false);}
static void landscape(void){ui_action_orientation(false);}
static void go_apps(lv_event_t*e){(void)e;load(s_apps);} static void go_light(lv_event_t*e){(void)e;load(s_light);} static void go_music(lv_event_t*e){(void)e;load(s_music);} static void go_devices(lv_event_t*e){(void)e;load(s_devices);}
static void go_remote(lv_event_t*e){(void)e;load(s_remote);}

static void pressed(lv_event_t*e){lv_indev_t*i=lv_indev_active();press_screen=lv_event_get_target(e);if(i)lv_indev_get_point(i,&press_start);}
static void released(lv_event_t*e){
 lv_obj_t*s=lv_event_get_target(e);lv_indev_t*i=lv_indev_active();if(!i||s!=press_screen)return;
 lv_point_t end;lv_indev_get_point(i,&end);int dx=end.x-press_start.x,dy=end.y-press_start.y;
 if(dx*dx+dy*dy<32*32)return;
lv_dir_t d=(dx<0?LV_DIR_LEFT:LV_DIR_RIGHT);if(dy*dy>dx*dx)d=(dy<0?LV_DIR_TOP:LV_DIR_BOTTOM);
 if(d==LV_DIR_BOTTOM&&press_start.y<=28&&s!=s_settings){return_screen=s;load(s_settings);return;}
 if(s==s_settings&&d==LV_DIR_TOP){load(return_screen?return_screen:s_home);return;}
 if(s==s_remote&&d==LV_DIR_RIGHT&&press_start.x<=36){load(s_apps);return;}
 if(s==s_home&&(d==LV_DIR_LEFT||d==LV_DIR_RIGHT)){load(s_apps);return;}
 if(s==s_apps&&d==LV_DIR_RIGHT&&press_start.x<=48){load(s_home);return;}
 if(s==s_light){if(d==LV_DIR_RIGHT&&press_start.x<=48){load(s_apps);return;}if(d==LV_DIR_LEFT||d==LV_DIR_RIGHT){light_index=(light_index+(d==LV_DIR_LEFT?1:2))%3;lv_label_set_text(light_title,lights[light_index]);ui_action_light_select(light_index);return;}}
 if(s!=s_home&&s!=s_apps&&s!=s_settings&&d==LV_DIR_RIGHT&&press_start.x<=48)load(s_apps);
}
static void gesture(lv_event_t*e){
lv_obj_t*s=lv_event_get_target(e);lv_indev_t*i=lv_indev_active();if(!i)return;lv_dir_t d=lv_indev_get_gesture_dir(i);
 if(d==LV_DIR_BOTTOM&&press_start.y<=28&&s!=s_settings){return_screen=s;load(s_settings);return;}
 if(s==s_settings&&d==LV_DIR_TOP){load(return_screen?return_screen:s_home);return;}
 if(s==s_remote&&d==LV_DIR_RIGHT&&press_start.x<=36){load(s_apps);return;}
 if(s==s_home&&(d==LV_DIR_LEFT||d==LV_DIR_RIGHT)){load(s_apps);return;}
 if(s==s_apps&&d==LV_DIR_RIGHT&&press_start.x<=48){load(s_home);return;}
 if(s==s_light){
   if(d==LV_DIR_RIGHT&&press_start.x<=48){load(s_apps);return;}
   if(d==LV_DIR_LEFT||d==LV_DIR_RIGHT){light_index=(light_index+(d==LV_DIR_LEFT?1:2))%3;lv_label_set_text(light_title,lights[light_index]);ui_action_light_select(light_index);return;}
 }
 if(s!=s_home&&s!=s_apps&&s!=s_settings&&d==LV_DIR_RIGHT&&press_start.x<=48)load(s_apps);
}
static void gestures(lv_obj_t*s){lv_obj_add_event_cb(s,pressed,LV_EVENT_PRESSED,NULL);lv_obj_add_event_cb(s,gesture,LV_EVENT_GESTURE,NULL);lv_obj_add_event_cb(s,released,LV_EVENT_RELEASED,NULL);}

static void slider_cb(lv_event_t*e){lv_obj_t*o=lv_event_get_target(e);int v=lv_slider_get_value(o);if(o==volume_slider)ui_action_volume(v);else if(o==brightness_slider)ui_action_brightness(v);else if(o==light_bri)ui_action_light_brightness(v);else if(o==light_temp)ui_action_light_temperature(v);else if(o==music_progress)ui_action_music_seek(v);}
static lv_obj_t *slider_row(lv_obj_t*p,const char*n,int y,int v){label(p,n,26,y-3,14);lv_obj_t*s=lv_slider_create(p);lv_obj_set_pos(s,150,y);lv_obj_set_size(s,450,10);lv_slider_set_value(s,v,LV_ANIM_OFF);lv_obj_set_style_bg_color(s,lv_color_hex(0x202A38),LV_PART_MAIN);lv_obj_set_style_bg_color(s,lv_color_hex(BLUE),LV_PART_INDICATOR);lv_obj_set_style_bg_color(s,lv_color_hex(TEXT),LV_PART_KNOB);lv_obj_add_event_cb(s,slider_cb,LV_EVENT_VALUE_CHANGED,NULL);return s;}
static void light_power_cb(lv_event_t*e){ui_action_light_power(lv_obj_has_state(lv_event_get_target(e),LV_STATE_CHECKED));}
static void preset_cb(lv_event_t*e){ui_action_light_preset((int)(intptr_t)lv_event_get_user_data(e));}
static void music_cb(lv_event_t*e){ui_music_action_t a=(ui_music_action_t)(intptr_t)lv_event_get_user_data(e);if(a==UI_MUSIC_PLAY_PAUSE){music_playing=!music_playing;lv_label_set_text(music_play_label,music_playing?LV_SYMBOL_PAUSE:LV_SYMBOL_PLAY);}ui_action_music(a);}
static void remote_cb(lv_event_t*e){ui_action_remote((ui_remote_action_t)(intptr_t)lv_event_get_user_data(e));}
static lv_obj_t *button(lv_obj_t*p,int x,int y,int w,int h,const char*t,lv_event_cb_t cb,intptr_t data){lv_obj_t*b=card(p,x,y,w,h);lv_obj_set_style_bg_color(b,lv_color_hex(0x1A2433),0);lv_obj_set_style_bg_color(b,lv_color_hex(0x24334A),LV_STATE_PRESSED);lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);lv_obj_t*l=label(b,t,0,0,14);lv_obj_center(l);lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void*)data);return b;}

static lv_obj_t *home_text(lv_obj_t *p,const char *t,int x,int y,const lv_font_t *font,lv_opa_t opa){
 lv_obj_t *o=lv_label_create(p);lv_label_set_text(o,t);lv_obj_set_pos(o,x,y);
 lv_obj_set_style_text_color(o,lv_color_hex(TEXT),0);lv_obj_set_style_text_opa(o,opa,0);
 lv_obj_set_style_text_font(o,font,0);return o;
}
static lv_obj_t *home_metric(lv_obj_t *p,const char *icon,const char *name,const char *value,int x){
 lv_obj_t *ic=home_text(p,icon,x,126,&lv_font_montserrat_20,LV_OPA_90);
 lv_obj_set_style_text_color(ic,lv_color_hex(0xBFE4FF),0);
 home_text(p,name,x+26,126,&lv_font_montserrat_14,LV_OPA_80);
 return home_text(p,value,x+26,145,&lv_font_montserrat_16,LV_OPA_COVER);
}
static void home_create(void){
 s_home=lv_obj_create(NULL);base(s_home);gestures(s_home);

 /* Full-screen weather scene.  The photographic asset layer will later map
    to the same resource id on Web Preview and /sdcard on ESP32. */
 lv_obj_set_style_bg_color(s_home,lv_color_hex(HOME_SKY_TOP),0);
 lv_obj_set_style_bg_grad_color(s_home,lv_color_hex(HOME_SKY_BOTTOM),0);
 lv_obj_set_style_bg_grad_dir(s_home,LV_GRAD_DIR_VER,0);

 /* warm horizon glow */
 lv_obj_t *glow=lv_obj_create(s_home);lv_obj_set_pos(glow,0,112);lv_obj_set_size(glow,640,60);
 lv_obj_set_style_border_width(glow,0,0);lv_obj_set_style_radius(glow,0,0);
 lv_obj_set_style_bg_color(glow,lv_color_hex(0x1E669B),0);
 lv_obj_set_style_bg_grad_color(glow,lv_color_hex(0xF0B35D),0);
 lv_obj_set_style_bg_grad_dir(glow,LV_GRAD_DIR_HOR,0);lv_obj_set_style_bg_opa(glow,LV_OPA_70,0);

 /* subtle glass shade behind weather information */
 lv_obj_t *shade=lv_obj_create(s_home);lv_obj_set_pos(shade,0,0);lv_obj_set_size(shade,250,172);
 lv_obj_set_style_border_width(shade,0,0);lv_obj_set_style_radius(shade,0,0);
 lv_obj_set_style_bg_color(shade,lv_color_hex(HOME_GLASS),0);lv_obj_set_style_bg_opa(shade,LV_OPA_40,0);

 /* left weather block */
 home_text(s_home,"WUHUA",18,12,&lv_font_montserrat_16,LV_OPA_COVER);
 lv_obj_t *sun=lv_obj_create(s_home);lv_obj_set_pos(sun,24,47);lv_obj_set_size(sun,38,38);
 lv_obj_set_style_radius(sun,LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(sun,0,0);
 lv_obj_set_style_bg_color(sun,lv_color_hex(HOME_SUN),0);lv_obj_set_style_shadow_width(sun,14,0);
 lv_obj_set_style_shadow_color(sun,lv_color_hex(HOME_SUN),0);lv_obj_set_style_shadow_opa(sun,LV_OPA_50,0);
 lv_obj_t *cloud=lv_obj_create(s_home);lv_obj_set_pos(cloud,43,68);lv_obj_set_size(cloud,52,24);
 lv_obj_set_style_radius(cloud,14,0);lv_obj_set_style_border_width(cloud,0,0);
 lv_obj_set_style_bg_color(cloud,lv_color_hex(0xF3F7FA),0);lv_obj_set_style_bg_opa(cloud,LV_OPA_90,0);

 home_text(s_home,"26°",112,42,&lv_font_montserrat_28,LV_OPA_COVER);
 home_text(s_home,"CLEAR",112,75,&lv_font_montserrat_14,LV_OPA_COVER);
 home_text(s_home,"H 28°   L 16°",112,98,&lv_font_montserrat_14,LV_OPA_80);

 lv_obj_t *rule=lv_obj_create(s_home);lv_obj_set_pos(rule,18,119);lv_obj_set_size(rule,214,1);
 lv_obj_set_style_border_width(rule,0,0);lv_obj_set_style_bg_color(rule,lv_color_hex(TEXT),0);
 lv_obj_set_style_bg_opa(rule,LV_OPA_20,0);

 home_metric(s_home,LV_SYMBOL_DOWN,"RAIN","10%",18);
 home_metric(s_home,LV_SYMBOL_REFRESH,"HUM","55%",92);
 home_metric(s_home,LV_SYMBOL_RIGHT,"WIND","2",166);

 /* divider */
 lv_obj_t *div=lv_obj_create(s_home);lv_obj_set_pos(div,249,16);lv_obj_set_size(div,1,140);
 lv_obj_set_style_border_width(div,0,0);lv_obj_set_style_bg_color(div,lv_color_hex(TEXT),0);
 lv_obj_set_style_bg_opa(div,LV_OPA_25,0);

 /* right date + dominant 12-hour clock */
 home_text(s_home,"SEP 24   WEDNESDAY",284,13,&lv_font_montserrat_20,LV_OPA_90);
 lv_obj_t *clock=home_text(s_home,"10:24",270,43,&lv_font_montserrat_48,LV_OPA_COVER);
 lv_obj_set_style_text_letter_space(clock,-2,0);
 home_text(s_home,"AM",535,91,&lv_font_montserrat_20,LV_OPA_90);
}
static lv_obj_t *app_tile(lv_obj_t*p,int x,uint32_t c1,uint32_t c2,const char *symbol,const char *name,lv_event_cb_t cb){
 lv_obj_t*b=lv_obj_create(p);lv_obj_set_pos(b,x,18);lv_obj_set_size(b,126,126);
 lv_obj_set_style_radius(b,26,0);lv_obj_set_style_border_width(b,0,0);
 lv_obj_set_style_bg_color(b,lv_color_hex(c1),0);lv_obj_set_style_bg_grad_color(b,lv_color_hex(c2),0);
 lv_obj_set_style_bg_grad_dir(b,LV_GRAD_DIR_VER,0);lv_obj_set_style_shadow_width(b,12,0);
 lv_obj_set_style_shadow_opa(b,LV_OPA_25,0);lv_obj_set_style_shadow_ofs_y(b,4,0);
 lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(b,LV_OBJ_FLAG_GESTURE_BUBBLE);
 lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,NULL);
 lv_obj_t*ic=home_text(b,symbol,0,0,&lv_font_montserrat_48,LV_OPA_COVER);
 lv_obj_set_style_text_color(ic,lv_color_hex(0xFFFFFF),0);lv_obj_center(ic);
 lv_obj_t*tx=home_text(p,name,0,146,&lv_font_montserrat_16,LV_OPA_COVER);
 lv_obj_align(tx,LV_ALIGN_TOP_LEFT,x+(126-lv_obj_get_width(tx))/2,146);
 return b;
}
static void apps_create(void){
 s_apps=lv_obj_create(NULL);base(s_apps);gestures(s_apps);
 /* Four large Apple-inspired launcher tiles. Labels deliberately sit outside
    the tiles so the 640x172 composition stays visually light. */
 app_tile(s_apps,18, 0xA9A9AE,0x62636A,LV_SYMBOL_LIST,"REMOTE",go_remote);
 app_tile(s_apps,173,0xFF315E,0xEF003F,LV_SYMBOL_AUDIO,"MUSIC",go_music);
 app_tile(s_apps,328,0xFFD72D,0xFFAA00,LV_SYMBOL_EYE_OPEN,"LIGHTS",go_light);
 app_tile(s_apps,483,0xFFFFFF,0xE9EBEE,LV_SYMBOL_HOME,"DEVICES",go_devices);
}
static void settings_create(void){s_settings=lv_obj_create(NULL);base(s_settings);gestures(s_settings);label(s_settings,"SYSTEM",24,14,14);label(s_settings,"Quick settings",96,12,20);volume_slider=slider_row(s_settings,"Volume",65,60);brightness_slider=slider_row(s_settings,"Brightness",108,80);lv_obj_t*h=label(s_settings,"Swipe up to close",255,148,14);lv_obj_set_style_text_color(h,lv_color_hex(MUTED),0);}
static void light_create(void){s_light=lv_obj_create(NULL);base(s_light);gestures(s_light);label(s_light,"‹",18,9,28);light_title=label(s_light,lights[0],54,13,20);lv_obj_t*hint=label(s_light,"Swipe lights  ·  edge swipe to back",220,17,14);lv_obj_set_style_text_color(hint,lv_color_hex(MUTED),0);light_switch=lv_switch_create(s_light);lv_obj_set_pos(light_switch,560,12);lv_obj_add_state(light_switch,LV_STATE_CHECKED);lv_obj_add_event_cb(light_switch,light_power_cb,LV_EVENT_VALUE_CHANGED,NULL);light_bri=slider_row(s_light,"Brightness",63,72);light_temp=slider_row(s_light,"Warm / Cool",102,58);const char*p[]={"Relax","Read","Movie","Night"};for(int n=0;n<4;n++)button(s_light,20+n*155,130,135,34,p[n],preset_cb,n);}
static void remote_create(void){
 s_remote=lv_obj_create(NULL);base(s_remote);gestures(s_remote);label(s_remote,"‹",14,10,28);label(s_remote,"REMOTE",48,15,14);
 lv_obj_t*pad=card(s_remote,108,22,250,136);
 button(pad,100,4,50,36,LV_SYMBOL_UP,remote_cb,UI_REMOTE_UP);
 button(pad,42,48,50,36,LV_SYMBOL_LEFT,remote_cb,UI_REMOTE_LEFT);
 button(pad,100,48,50,36,"OK",remote_cb,UI_REMOTE_OK);
 button(pad,158,48,50,36,LV_SYMBOL_RIGHT,remote_cb,UI_REMOTE_RIGHT);
 button(pad,100,92,50,36,LV_SYMBOL_DOWN,remote_cb,UI_REMOTE_DOWN);
 button(s_remote,382,22,72,40,"Back",remote_cb,UI_REMOTE_BACK);
 button(s_remote,462,22,72,40,LV_SYMBOL_HOME,remote_cb,UI_REMOTE_HOME);
 button(s_remote,542,22,82,40,LV_SYMBOL_POWER,remote_cb,UI_REMOTE_POWER);
 button(s_remote,382,70,112,40,LV_SYMBOL_PLAY,remote_cb,UI_REMOTE_PLAY_PAUSE);
 button(s_remote,502,70,58,40,"Vol-",remote_cb,UI_REMOTE_VOL_DOWN);
 button(s_remote,566,70,58,40,"Vol+",remote_cb,UI_REMOTE_VOL_UP);
 lv_obj_t*h=label(s_remote,"edge swipe  >  back",394,136,14);lv_obj_set_style_text_color(h,lv_color_hex(MUTED),0);
}
static void music_create(void){s_music=lv_obj_create(NULL);base(s_music);gestures(s_music);lv_obj_t*c=card(s_music,16,16,136,136);lv_obj_t*ic=label(c,LV_SYMBOL_AUDIO,50,40,28);lv_obj_set_style_text_color(ic,lv_color_hex(BLUE),0);label(s_music,"Time Traveler",184,22,20);lv_obj_t*a=label(s_music,"Zhou Shen  ·  Local library",184,52,14);lv_obj_set_style_text_color(a,lv_color_hex(MUTED),0);music_progress=lv_slider_create(s_music);lv_obj_set_pos(music_progress,184,84);lv_obj_set_size(music_progress,420,8);lv_slider_set_value(music_progress,36,LV_ANIM_OFF);lv_obj_add_event_cb(music_progress,slider_cb,LV_EVENT_VALUE_CHANGED,NULL);button(s_music,316,112,54,46,LV_SYMBOL_PREV,music_cb,UI_MUSIC_PREV);lv_obj_t*b=button(s_music,382,108,62,54,"",music_cb,UI_MUSIC_PLAY_PAUSE);music_play_label=label(b,LV_SYMBOL_PAUSE,0,0,20);lv_obj_center(music_play_label);button(s_music,456,112,54,46,LV_SYMBOL_NEXT,music_cb,UI_MUSIC_NEXT);}
static void devices_create(void){s_devices=lv_obj_create(NULL);base(s_devices);gestures(s_devices);label(s_devices,"DEVICES",24,14,14);lv_obj_t*c=card(s_devices,176,36,288,112);lv_obj_t*i=label(c,LV_SYMBOL_HOME,120,10,28);lv_obj_set_style_text_color(i,lv_color_hex(BLUE),0);label(c,"No devices yet",82,48,16);lv_obj_t*t=label(c,"Reserved for HA / robot devices",32,76,14);lv_obj_set_style_text_color(t,lv_color_hex(MUTED),0);}

void ui_init(void){home_create();apps_create();settings_create();light_create();remote_create();music_create();devices_create();return_screen=s_home;lv_screen_load(s_home);}
void ui_show_home(void){load(s_home);}void ui_show_functions(void){load(s_apps);}void ui_show_settings(void){return_screen=lv_screen_active();load(s_settings);}
