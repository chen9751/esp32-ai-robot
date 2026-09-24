#include "ui_manager.h"
#include "lvgl.h"

#define BG 0x080B10
#define CARD 0x151A22
#define BLUE 0x3B82F6
#define TEXT 0xF4F7FB
#define MUTED 0x9AA6B2

static lv_obj_t *s_home,*s_apps,*s_settings,*s_light,*s_remote,*s_music,*s_devices;
static lv_obj_t *return_screen;
static lv_obj_t *light_title,*light_switch,*light_bri,*light_temp;
static lv_obj_t *music_play_label,*music_progress;
static lv_obj_t *volume_slider,*brightness_slider;
static int light_index;
static bool music_playing=true;
static const char *lights[]={"Living","Desk","Bedroom"};

__attribute__((weak)) void ui_action_volume(int value){(void)value;}
__attribute__((weak)) void ui_action_brightness(int value){(void)value;}
__attribute__((weak)) void ui_action_light_select(int index){(void)index;}
__attribute__((weak)) void ui_action_light_power(bool on){(void)on;}
__attribute__((weak)) void ui_action_light_brightness(int value){(void)value;}
__attribute__((weak)) void ui_action_light_temperature(int value){(void)value;}
__attribute__((weak)) void ui_action_light_preset(int preset){(void)preset;}
__attribute__((weak)) void ui_action_music(ui_music_action_t action){(void)action;}
__attribute__((weak)) void ui_action_music_seek(int percent){(void)percent;}
__attribute__((weak)) void ui_action_remote(ui_remote_action_t action){(void)action;}

static void base(lv_obj_t *s){lv_obj_set_style_bg_color(s,lv_color_hex(BG),0);lv_obj_set_style_bg_opa(s,LV_OPA_COVER,0);lv_obj_set_style_text_color(s,lv_color_hex(TEXT),0);lv_obj_set_style_border_width(s,0,0);lv_obj_set_style_pad_all(s,0,0);}
static lv_obj_t *label(lv_obj_t*p,const char*t,int x,int y,int z){lv_obj_t*o=lv_label_create(p);lv_label_set_text(o,t);lv_obj_set_pos(o,x,y);lv_obj_set_style_text_color(o,lv_color_hex(TEXT),0);if(z>=28)lv_obj_set_style_text_font(o,&lv_font_montserrat_28,0);else if(z>=20)lv_obj_set_style_text_font(o,&lv_font_montserrat_20,0);else if(z>=16)lv_obj_set_style_text_font(o,&lv_font_montserrat_16,0);else lv_obj_set_style_text_font(o,&lv_font_montserrat_14,0);return o;}
static lv_obj_t *card(lv_obj_t*p,int x,int y,int w,int h){lv_obj_t*o=lv_obj_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_radius(o,14,0);lv_obj_set_style_bg_color(o,lv_color_hex(CARD),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(0x2A3340),0);lv_obj_set_style_pad_all(o,8,0);return o;}
static void load(lv_obj_t*s){lv_screen_load_anim(s,LV_SCR_LOAD_ANIM_FADE_IN,140,0,false);}
static void go_apps(lv_event_t*e){(void)e;load(s_apps);} static void go_light(lv_event_t*e){(void)e;load(s_light);} static void go_remote(lv_event_t*e){(void)e;load(s_remote);} static void go_music(lv_event_t*e){(void)e;load(s_music);} static void go_devices(lv_event_t*e){(void)e;load(s_devices);}

static void gesture(lv_event_t*e){
 lv_obj_t*s=lv_event_get_target(e);lv_indev_t*in=lv_indev_active();if(!in)return;lv_dir_t d=lv_indev_get_gesture_dir(in);lv_point_t p;lv_indev_get_point(in,&p);
 if(d==LV_DIR_BOTTOM&&p.y<45&&s!=s_settings){return_screen=s;load(s_settings);return;}
 if(s==s_settings&&d==LV_DIR_TOP){load(return_screen?return_screen:s_home);return;}
 if(s==s_home&&(d==LV_DIR_LEFT||d==LV_DIR_RIGHT)){load(s_apps);return;}
 if(s==s_apps&&d==LV_DIR_RIGHT){load(s_home);return;}
 if(s==s_light&&(d==LV_DIR_LEFT||d==LV_DIR_RIGHT)){light_index=(light_index+(d==LV_DIR_LEFT?1:2))%3;lv_label_set_text(light_title,lights[light_index]);ui_action_light_select(light_index);return;}
 if(s!=s_home&&s!=s_apps&&s!=s_settings&&d==LV_DIR_RIGHT)load(s_apps);
}
static void gestures(lv_obj_t*s){lv_obj_add_event_cb(s,gesture,LV_EVENT_GESTURE,NULL);}

static void slider_cb(lv_event_t*e){lv_obj_t*o=lv_event_get_target(e);int v=lv_slider_get_value(o);if(o==volume_slider)ui_action_volume(v);else if(o==brightness_slider)ui_action_brightness(v);else if(o==light_bri)ui_action_light_brightness(v);else if(o==light_temp)ui_action_light_temperature(v);else if(o==music_progress)ui_action_music_seek(v);}
static lv_obj_t *slider_row(lv_obj_t*p,const char*n,int y,int v){label(p,n,14,y,14);lv_obj_t*s=lv_slider_create(p);lv_obj_set_pos(s,100,y+2);lv_obj_set_size(s,190,12);lv_slider_set_value(s,v,LV_ANIM_OFF);lv_obj_set_style_bg_color(s,lv_color_hex(BLUE),LV_PART_INDICATOR);lv_obj_add_event_cb(s,slider_cb,LV_EVENT_VALUE_CHANGED,NULL);return s;}

static void light_power_cb(lv_event_t*e){ui_action_light_power(lv_obj_has_state(lv_event_get_target(e),LV_STATE_CHECKED));}
static void preset_cb(lv_event_t*e){ui_action_light_preset((int)(intptr_t)lv_event_get_user_data(e));}
static void music_cb(lv_event_t*e){ui_music_action_t a=(ui_music_action_t)(intptr_t)lv_event_get_user_data(e);if(a==UI_MUSIC_PLAY_PAUSE){music_playing=!music_playing;lv_label_set_text(music_play_label,music_playing?LV_SYMBOL_PAUSE:LV_SYMBOL_PLAY);}ui_action_music(a);}
static void remote_cb(lv_event_t*e){ui_action_remote((ui_remote_action_t)(intptr_t)lv_event_get_user_data(e));}
static lv_obj_t *click_card(lv_obj_t*p,int x,int y,int w,int h,const char*t,lv_event_cb_t cb,intptr_t data){lv_obj_t*b=card(p,x,y,w,h);lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);label(b,t,4,2,14);lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void*)data);return b;}

static void home_create(void){s_home=lv_obj_create(NULL);base(s_home);gestures(s_home);lv_obj_t*w=card(s_home,8,10,92,152);label(w,LV_SYMBOL_EYE_OPEN,8,6,20);label(w,"26 C",8,35,28);label(w,"Cloudy",8,72,14);lv_obj_t*m=label(w,"68% AQI 32",8,108,14);lv_obj_set_style_text_color(m,lv_color_hex(MUTED),0);label(s_home,"SEP 24 THU",118,27,16);label(s_home,"12:53",116,70,28);lv_obj_t*h=label(s_home,"Swipe > Apps",180,142,14);lv_obj_set_style_text_color(h,lv_color_hex(MUTED),0);}
static void app_btn(lv_obj_t*p,const char*i,const char*t,int x,lv_event_cb_t cb){lv_obj_t*b=card(p,x,35,70,100);lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,NULL);lv_obj_t*ic=label(b,i,18,12,28);lv_obj_set_style_text_color(ic,lv_color_hex(BLUE),0);label(b,t,6,62,14);}
static void apps_create(void){s_apps=lv_obj_create(NULL);base(s_apps);gestures(s_apps);label(s_apps,"HOME",10,8,14);label(s_apps,"Functions",118,8,16);app_btn(s_apps,LV_SYMBOL_LIST,"Remote",10,go_remote);app_btn(s_apps,LV_SYMBOL_AUDIO,"Music",88,go_music);app_btn(s_apps,LV_SYMBOL_EYE_OPEN,"Lights",166,go_light);app_btn(s_apps,LV_SYMBOL_HOME,"Devices",244,go_devices);}
static void settings_create(void){s_settings=lv_obj_create(NULL);base(s_settings);gestures(s_settings);label(s_settings,"System settings",12,10,20);volume_slider=slider_row(s_settings,"Volume",52,60);brightness_slider=slider_row(s_settings,"Brightness",96,80);lv_obj_t*h=label(s_settings,"Swipe up to close",96,145,14);lv_obj_set_style_text_color(h,lv_color_hex(MUTED),0);}
static void light_create(void){s_light=lv_obj_create(NULL);base(s_light);gestures(s_light);label(s_light,"<",8,8,20);light_title=label(s_light,lights[0],38,10,20);light_switch=lv_switch_create(s_light);lv_obj_set_pos(light_switch,260,10);lv_obj_add_state(light_switch,LV_STATE_CHECKED);lv_obj_add_event_cb(light_switch,light_power_cb,LV_EVENT_VALUE_CHANGED,NULL);light_bri=slider_row(s_light,"Brightness",55,72);light_temp=slider_row(s_light,"Warm / Cool",91,58);const char*p[]={"Relax","Read","Movie","Night"};for(int i=0;i<4;i++)click_card(s_light,8+i*78,128,70,36,p[i],preset_cb,i);}
static lv_obj_t *rbtn(int x,int y,int w,const char*t,ui_remote_action_t a){return click_card(s_remote,x,y,w,34,t,remote_cb,a);}
static void remote_create(void){s_remote=lv_obj_create(NULL);base(s_remote);gestures(s_remote);label(s_remote,"TV Remote",108,4,16);rbtn(126,28,68,LV_SYMBOL_UP,UI_REMOTE_UP);rbtn(50,64,68,LV_SYMBOL_LEFT,UI_REMOTE_LEFT);rbtn(126,64,68,"OK",UI_REMOTE_OK);rbtn(202,64,68,LV_SYMBOL_RIGHT,UI_REMOTE_RIGHT);rbtn(126,100,68,LV_SYMBOL_DOWN,UI_REMOTE_DOWN);rbtn(12,136,56,"Back",UI_REMOTE_BACK);rbtn(74,136,56,LV_SYMBOL_HOME,UI_REMOTE_HOME);rbtn(136,136,56,LV_SYMBOL_PLAY,UI_REMOTE_PLAY_PAUSE);rbtn(198,136,50,"Vol+",UI_REMOTE_VOL_UP);rbtn(254,136,50,LV_SYMBOL_POWER,UI_REMOTE_POWER);}
static void music_create(void){s_music=lv_obj_create(NULL);base(s_music);gestures(s_music);lv_obj_t*c=card(s_music,10,14,104,104);label(c,LV_SYMBOL_AUDIO,30,25,28);label(s_music,"Time Traveler",130,20,20);lv_obj_t*a=label(s_music,"Zhou Shen",130,48,14);lv_obj_set_style_text_color(a,lv_color_hex(MUTED),0);music_progress=lv_slider_create(s_music);lv_obj_set_pos(music_progress,130,78);lv_obj_set_size(music_progress,176,10);lv_slider_set_value(music_progress,36,LV_ANIM_OFF);lv_obj_add_event_cb(music_progress,slider_cb,LV_EVENT_VALUE_CHANGED,NULL);click_card(s_music,142,119,42,42,LV_SYMBOL_PREV,music_cb,UI_MUSIC_PREV);lv_obj_t*b=click_card(s_music,194,115,50,50,"",music_cb,UI_MUSIC_PLAY_PAUSE);music_play_label=label(b,LV_SYMBOL_PAUSE,8,5,20);click_card(s_music,254,119,42,42,LV_SYMBOL_NEXT,music_cb,UI_MUSIC_NEXT);}
static void devices_create(void){s_devices=lv_obj_create(NULL);base(s_devices);gestures(s_devices);label(s_devices,"Devices",12,10,20);label(s_devices,LV_SYMBOL_HOME,144,48,28);label(s_devices,"No devices yet",106,92,16);lv_obj_t*t=label(s_devices,"Reserved for HA / robot devices",58,124,14);lv_obj_set_style_text_color(t,lv_color_hex(MUTED),0);}

void ui_init(void){home_create();apps_create();settings_create();light_create();remote_create();music_create();devices_create();return_screen=s_home;lv_screen_load(s_home);}
void ui_show_home(void){load(s_home);}void ui_show_functions(void){load(s_apps);}void ui_show_settings(void){return_screen=lv_screen_active();load(s_settings);}
