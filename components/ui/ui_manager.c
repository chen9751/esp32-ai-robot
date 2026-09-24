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
static lv_obj_t *return_screen;
static lv_obj_t *music_play_label,*music_progress,*volume_slider,*brightness_slider;
static int light_index; static bool music_playing=true; static lv_point_t press_start; static lv_obj_t *press_screen;

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

static void base(lv_obj_t*s){lv_obj_set_scrollbar_mode(s,LV_SCROLLBAR_MODE_OFF);lv_obj_set_style_bg_color(s,lv_color_hex(BG),0);lv_obj_set_style_bg_opa(s,LV_OPA_COVER,0);lv_obj_set_style_text_color(s,lv_color_hex(TEXT),0);lv_obj_set_style_border_width(s,0,0);lv_obj_set_style_pad_all(s,0,0);}
static lv_obj_t *label(lv_obj_t*p,const char*t,int x,int y,int z){lv_obj_t*o=lv_label_create(p);lv_label_set_text(o,t);lv_obj_set_pos(o,x,y);lv_obj_set_style_text_color(o,lv_color_hex(TEXT),0);lv_obj_set_style_text_font(o,z>=28?&lv_font_montserrat_28:z>=20?&lv_font_montserrat_20:z>=16?&lv_font_montserrat_16:&lv_font_montserrat_14,0);return o;}
static lv_obj_t *card(lv_obj_t*p,int x,int y,int w,int h){lv_obj_t*o=lv_obj_create(p);lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_radius(o,18,0);lv_obj_set_style_bg_color(o,lv_color_hex(CARD),0);lv_obj_set_style_bg_grad_color(o,lv_color_hex(CARD2),0);lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_HOR,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(LINE),0);lv_obj_set_style_pad_all(o,10,0);return o;}
static void load(lv_obj_t*s){if(!s||lv_screen_active()==s)return;lv_screen_load(s);}\nstatic void landscape(void){ui_action_orientation(false);}
static void go_apps(lv_event_t*e){(void)e;load(s_apps);} static void go_light(lv_event_t*e){(void)e;load(s_light);} static void go_music(lv_event_t*e){(void)e;load(s_music);} static void go_devices(lv_event_t*e){(void)e;load(s_devices);}
static void go_remote(lv_event_t*e){(void)e;load(s_remote);}

static bool swipe_handled;

static bool handle_swipe(lv_obj_t*s,lv_dir_t d){
 if(d==LV_DIR_BOTTOM&&press_start.y<=28&&s==s_home){return_screen=s_home;load(s_settings);return true;}
 if(s==s_settings&&d==LV_DIR_TOP){load(return_screen?return_screen:s_home);return true;}
 if(s==s_remote&&d==LV_DIR_RIGHT&&press_start.x<=36){load(s_apps);return true;}
 if(s==s_home&&(d==LV_DIR_LEFT||d==LV_DIR_RIGHT)){load(s_apps);return true;}
 if(s==s_apps&&d==LV_DIR_RIGHT&&press_start.x<=48){load(s_home);return true;}
 /* Lights owns horizontal carousel gestures itself. Only left-edge back is global. */
 if(s==s_light&&d==LV_DIR_RIGHT&&press_start.x<=48){load(s_apps);return true;}
 if(s!=s_home&&s!=s_apps&&s!=s_settings&&s!=s_light&&d==LV_DIR_RIGHT&&press_start.x<=48){load(s_apps);return true;}
 return false;
}

static void pressed(lv_event_t*e){
 lv_indev_t*i=lv_indev_active();
 press_screen=lv_event_get_current_target(e);
 swipe_handled=false;
 if(i)lv_indev_get_point(i,&press_start);
}
static void gesture(lv_event_t*e){
 if(swipe_handled)return;
 lv_indev_t*i=lv_indev_active();if(!i)return;
 lv_obj_t*s=lv_event_get_current_target(e);
 swipe_handled=handle_swipe(s,lv_indev_get_gesture_dir(i));
}
static void released(lv_event_t*e){
 if(swipe_handled)return;
 lv_indev_t*i=lv_indev_active();if(!i)return;
 lv_obj_t*s=lv_event_get_current_target(e);
 if(s!=press_screen)return;
 lv_point_t end;lv_indev_get_point(i,&end);
 int dx=end.x-press_start.x,dy=end.y-press_start.y;
 if(dx*dx+dy*dy<32*32)return;
 lv_dir_t d=(dx<0?LV_DIR_LEFT:LV_DIR_RIGHT);
 if(dy*dy>dx*dx)d=(dy<0?LV_DIR_TOP:LV_DIR_BOTTOM);
 swipe_handled=handle_swipe(s,d);
}
static void gestures(lv_obj_t*s){
 lv_obj_add_event_cb(s,pressed,LV_EVENT_PRESSED,NULL);
 lv_obj_add_event_cb(s,gesture,LV_EVENT_GESTURE,NULL);
 lv_obj_add_event_cb(s,released,LV_EVENT_RELEASED,NULL);
}

static void slider_cb(lv_event_t*e){lv_obj_t*o=lv_event_get_target(e);int v=lv_slider_get_value(o);if(o==volume_slider)ui_action_volume(v);else if(o==brightness_slider)ui_action_brightness(v);else if(o==music_progress)ui_action_music_seek(v);}
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
 lv_obj_t *glow=lv_obj_create(s_home);lv_obj_set_scrollbar_mode(glow,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(glow,0,112);lv_obj_set_size(glow,640,60);
 lv_obj_set_style_border_width(glow,0,0);lv_obj_set_style_radius(glow,0,0);
 lv_obj_set_style_bg_color(glow,lv_color_hex(0x1E669B),0);
 lv_obj_set_style_bg_grad_color(glow,lv_color_hex(0xF0B35D),0);
 lv_obj_set_style_bg_grad_dir(glow,LV_GRAD_DIR_HOR,0);lv_obj_set_style_bg_opa(glow,LV_OPA_70,0);

 /* subtle glass shade behind weather information */
 lv_obj_t *shade=lv_obj_create(s_home);lv_obj_set_scrollbar_mode(shade,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(shade,0,0);lv_obj_set_size(shade,250,172);
 lv_obj_set_style_border_width(shade,0,0);lv_obj_set_style_radius(shade,0,0);
 lv_obj_set_style_bg_color(shade,lv_color_hex(HOME_GLASS),0);lv_obj_set_style_bg_opa(shade,LV_OPA_40,0);

 /* left weather block */
 home_text(s_home,"WUHUA",18,12,&lv_font_montserrat_16,LV_OPA_COVER);
 lv_obj_t *sun=lv_obj_create(s_home);lv_obj_set_scrollbar_mode(sun,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(sun,24,47);lv_obj_set_size(sun,38,38);
 lv_obj_set_style_radius(sun,LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(sun,0,0);
 lv_obj_set_style_bg_color(sun,lv_color_hex(HOME_SUN),0);lv_obj_set_style_shadow_width(sun,14,0);
 lv_obj_set_style_shadow_color(sun,lv_color_hex(HOME_SUN),0);lv_obj_set_style_shadow_opa(sun,LV_OPA_50,0);
 lv_obj_t *cloud=lv_obj_create(s_home);lv_obj_set_scrollbar_mode(cloud,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(cloud,43,68);lv_obj_set_size(cloud,52,24);
 lv_obj_set_style_radius(cloud,14,0);lv_obj_set_style_border_width(cloud,0,0);
 lv_obj_set_style_bg_color(cloud,lv_color_hex(0xF3F7FA),0);lv_obj_set_style_bg_opa(cloud,LV_OPA_90,0);

 home_text(s_home,"26°",112,42,&lv_font_montserrat_28,LV_OPA_COVER);
 home_text(s_home,"CLEAR",112,75,&lv_font_montserrat_14,LV_OPA_COVER);
 home_text(s_home,"H 28°   L 16°",112,98,&lv_font_montserrat_14,LV_OPA_80);

 lv_obj_t *rule=lv_obj_create(s_home);lv_obj_set_scrollbar_mode(rule,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(rule,18,119);lv_obj_set_size(rule,214,1);
 lv_obj_set_style_border_width(rule,0,0);lv_obj_set_style_bg_color(rule,lv_color_hex(TEXT),0);
 lv_obj_set_style_bg_opa(rule,LV_OPA_20,0);

 home_metric(s_home,LV_SYMBOL_DOWN,"RAIN","10%",18);
 home_metric(s_home,LV_SYMBOL_REFRESH,"HUM","55%",92);
 home_metric(s_home,LV_SYMBOL_RIGHT,"WIND","2",166);

 /* divider */
 lv_obj_t *div=lv_obj_create(s_home);lv_obj_set_scrollbar_mode(div,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(div,249,16);lv_obj_set_size(div,1,140);
 lv_obj_set_style_border_width(div,0,0);lv_obj_set_style_bg_color(div,lv_color_hex(TEXT),0);
 lv_obj_set_style_bg_opa(div,LV_OPA_20,0);

 /* right date + dominant 12-hour clock */
 home_text(s_home,"SEP 24   WEDNESDAY",284,13,&lv_font_montserrat_20,LV_OPA_90);
 lv_obj_t *clock=home_text(s_home,"10:24",270,43,&lv_font_montserrat_48,LV_OPA_COVER);
 lv_obj_set_style_text_letter_space(clock,-2,0);
 home_text(s_home,"AM",535,91,&lv_font_montserrat_20,LV_OPA_90);
}
typedef enum {APP_REMOTE,APP_MUSIC,APP_LIGHTS,APP_DEVICES} app_icon_t;

static lv_obj_t *app_icon_box(lv_obj_t*p,int x,int y,int w,int h,int r,uint32_t color){
 lv_obj_t*o=lv_obj_create(p);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);
 lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_radius(o,r,0);lv_obj_set_style_border_width(o,0,0);
 lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_add_flag(o,LV_OBJ_FLAG_EVENT_BUBBLE);return o;
}
static void app_remote_icon(lv_obj_t*p){
 lv_obj_t*body=app_icon_box(p,48,25,36,82,15,0xFFFFFF);
 lv_obj_t*top=app_icon_box(body,10,12,16,16,8,0x6E7581);(void)top;
 app_icon_box(body,14,38,8,8,4,0x6E7581);
 app_icon_box(body,8,53,20,6,3,0x6E7581);
 app_icon_box(body,8,66,20,6,3,0x6E7581);
}
static void app_bulb_icon(lv_obj_t*p){
 lv_obj_t*bulb=app_icon_box(p,37,25,58,58,29,0xFFFFFF);
 lv_obj_t*cut=app_icon_box(bulb,14,35,30,23,8,0xFFB300);(void)cut;
 app_icon_box(p,49,81,34,10,5,0xFFFFFF);
 app_icon_box(p,54,94,24,7,3,0xFFFFFF);
}
static lv_obj_t *app_tile(lv_obj_t*p,int x,uint32_t c1,uint32_t c2,app_icon_t type,lv_event_cb_t cb){
 lv_obj_t*b=lv_obj_create(p);lv_obj_set_scrollbar_mode(b,LV_SCROLLBAR_MODE_OFF);lv_obj_remove_flag(b,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_set_pos(b,x,20);lv_obj_set_size(b,132,132);
 lv_obj_set_style_radius(b,28,0);lv_obj_set_style_border_width(b,1,0);
 lv_obj_set_style_border_color(b,lv_color_hex(0x31405A),0);
 lv_obj_set_style_bg_color(b,lv_color_hex(c1),0);lv_obj_set_style_bg_grad_color(b,lv_color_hex(c2),0);
 lv_obj_set_style_bg_grad_dir(b,LV_GRAD_DIR_VER,0);lv_obj_set_style_shadow_width(b,12,0);
 lv_obj_set_style_shadow_opa(b,LV_OPA_20,0);lv_obj_set_style_shadow_ofs_y(b,4,0);
 lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(b,LV_OBJ_FLAG_GESTURE_BUBBLE);
 lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,NULL);
 if(type==APP_REMOTE)app_remote_icon(b);
 else if(type==APP_LIGHTS)app_bulb_icon(b);
 else{
  const char*symbol=type==APP_MUSIC?LV_SYMBOL_AUDIO:LV_SYMBOL_HOME;
  lv_obj_t*ic=home_text(b,symbol,0,0,&lv_font_montserrat_48,LV_OPA_COVER);
  lv_obj_set_style_text_color(ic,lv_color_hex(type==APP_DEVICES?0xFF9500:0xFFFFFF),0);
  lv_obj_set_style_transform_scale(ic,300,0);lv_obj_center(ic);lv_obj_add_flag(ic,LV_OBJ_FLAG_EVENT_BUBBLE);
 }
 return b;
}
static void apps_create(void){
 s_apps=lv_obj_create(NULL);base(s_apps);gestures(s_apps);
 /* Four centered square app icons, no labels. */
 app_tile(s_apps,32, 0x9FA2AA,0x62636A,APP_REMOTE,go_remote);
 app_tile(s_apps,180,0xFF315E,0xEF003F,APP_MUSIC,go_music);
 app_tile(s_apps,328,0xFFD72D,0xFFAA00,APP_LIGHTS,go_light);
 app_tile(s_apps,476,0xFFFFFF,0xEEF0F4,APP_DEVICES,go_devices);
}
static void quick_toggle_cb(lv_event_t*e){lv_obj_t*o=lv_event_get_target(e);bool on=lv_obj_has_state(o,LV_STATE_CHECKED);
 lv_obj_set_style_bg_color(o,lv_color_hex(on?0x244BC8:0x172236),0);lv_obj_set_style_shadow_opa(o,on?LV_OPA_30:LV_OPA_10,0);}
static lv_obj_t *quick_toggle(lv_obj_t*p,int x,int y,const char*icon,bool on){
 lv_obj_t*o=lv_obj_create(p);lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,62,62);lv_obj_set_style_radius(o,18,0);
 lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(0x344A70),0);
 lv_obj_set_style_bg_color(o,lv_color_hex(on?0x244BC8:0x172236),0);lv_obj_set_style_shadow_width(o,10,0);
 lv_obj_set_style_shadow_color(o,lv_color_hex(0x5A5CFF),0);lv_obj_set_style_shadow_opa(o,on?LV_OPA_30:LV_OPA_10,0);
 lv_obj_add_flag(o,LV_OBJ_FLAG_CHECKABLE);if(on)lv_obj_add_state(o,LV_STATE_CHECKED);
 lv_obj_add_event_cb(o,quick_toggle_cb,LV_EVENT_VALUE_CHANGED,NULL);
 lv_obj_t*i=home_text(o,icon,0,0,&lv_font_montserrat_28,LV_OPA_COVER);lv_obj_center(i);return o;
}
static lv_obj_t *control_slider(lv_obj_t*p,int y,const char*icon,int value,bool warm){
 lv_obj_t*s=lv_slider_create(p);lv_obj_set_pos(s,28,y);lv_obj_set_size(s,372,58);
 lv_slider_set_range(s,0,100);lv_slider_set_value(s,value,LV_ANIM_OFF);
 lv_obj_set_style_radius(s,29,LV_PART_MAIN);lv_obj_set_style_bg_color(s,lv_color_hex(0x111D30),LV_PART_MAIN);
 lv_obj_set_style_bg_opa(s,LV_OPA_90,LV_PART_MAIN);lv_obj_set_style_radius(s,29,LV_PART_INDICATOR);
 lv_obj_set_style_bg_color(s,lv_color_hex(warm?0xFFE0A0:0x666DFF),LV_PART_INDICATOR);
 lv_obj_set_style_bg_opa(s,LV_OPA_COVER,LV_PART_INDICATOR);
 lv_obj_set_style_bg_opa(s,LV_OPA_TRANSP,LV_PART_KNOB);lv_obj_set_style_pad_all(s,0,LV_PART_KNOB);
 lv_obj_t*i=home_text(p,icon,46,y+15,&lv_font_montserrat_28,LV_OPA_COVER);lv_obj_add_flag(i,LV_OBJ_FLAG_EVENT_BUBBLE);
 lv_obj_add_event_cb(s,slider_cb,LV_EVENT_VALUE_CHANGED,NULL);return s;
}
static void settings_create(void){
 s_settings=lv_obj_create(NULL);base(s_settings);gestures(s_settings);
 /* Control Center is reachable only from Home. It visually behaves as a top overlay. */
 lv_obj_set_style_bg_color(s_settings,lv_color_hex(0x07111F),0);lv_obj_set_style_bg_grad_color(s_settings,lv_color_hex(0x14264A),0);lv_obj_set_style_bg_grad_dir(s_settings,LV_GRAD_DIR_HOR,0);
 lv_obj_t*handle=lv_obj_create(s_settings);lv_obj_set_scrollbar_mode(handle,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(handle,292,5);lv_obj_set_size(handle,56,5);lv_obj_set_style_radius(handle,3,0);lv_obj_set_style_border_width(handle,0,0);lv_obj_set_style_bg_color(handle,lv_color_hex(0xAEB8D0),0);
 brightness_slider=control_slider(s_settings,18,LV_SYMBOL_EYE_OPEN,72,true);
 volume_slider=control_slider(s_settings,92,LV_SYMBOL_VOLUME_MAX,60,false);
 quick_toggle(s_settings,424,18,LV_SYMBOL_WIFI,true);
 quick_toggle(s_settings,498,18,LV_SYMBOL_MUTE,false);
 quick_toggle(s_settings,424,92,"C",false);
 quick_toggle(s_settings,498,92,"A",true);
}
#define LIGHT_ROOM_COUNT 4
#define LIGHT_CARD_W 312
#define LIGHT_CARD_H 146
#define LIGHT_CARD_GAP 14
static const char *room_names[LIGHT_ROOM_COUNT]={"LIVING","STUDY","BEDROOM","SMALL BED"};
static bool room_power[LIGHT_ROOM_COUNT]={true,true,true,true};

static void room_scene_style(lv_obj_t *o,int room){
 static const uint32_t warm[LIGHT_ROOM_COUNT]={0x9B542C,0x745038,0x87513A,0x6B4C3B};
 lv_obj_set_style_bg_color(o,lv_color_hex(warm[room]),0);
 lv_obj_set_style_bg_grad_color(o,lv_color_hex(0x17243A),0);
 lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_HOR,0);
}
static lv_obj_t *glyph_box(lv_obj_t*p,int x,int y,int w,int h,int r,lv_opa_t opa){
 lv_obj_t*o=lv_obj_create(p);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);
 lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_radius(o,r,0);lv_obj_set_style_border_width(o,0,0);
 lv_obj_set_style_bg_color(o,lv_color_hex(0xFFFFFF),0);lv_obj_set_style_bg_opa(o,opa,0);
 lv_obj_add_flag(o,LV_OBJ_FLAG_EVENT_BUBBLE);return o;
}
static void room_furniture_glyph(lv_obj_t*p,int room){
 /* common floor lamp */
 glyph_box(p,16,56,4,31,2,LV_OPA_80);
 glyph_box(p,8,49,20,10,5,LV_OPA_80);
 glyph_box(p,11,86,14,3,1,LV_OPA_80);
 if(room==0){
  glyph_box(p,48,71,54,19,7,LV_OPA_80);glyph_box(p,43,77,7,16,3,LV_OPA_80);
  glyph_box(p,100,77,7,16,3,LV_OPA_80);glyph_box(p,53,64,19,12,4,LV_OPA_60);glyph_box(p,75,64,19,12,4,LV_OPA_60);
 }else if(room==1){
  glyph_box(p,47,57,56,34,5,LV_OPA_80);glyph_box(p,52,62,46,23,2,LV_OPA_20);
  glyph_box(p,73,91,5,9,1,LV_OPA_80);glyph_box(p,63,98,25,3,1,LV_OPA_80);
 }else if(room==2){
  glyph_box(p,46,74,60,17,5,LV_OPA_80);glyph_box(p,51,67,20,11,4,LV_OPA_60);
  glyph_box(p,42,66,5,29,1,LV_OPA_80);glyph_box(p,103,86,5,9,1,LV_OPA_80);
 }else{
  glyph_box(p,50,57,54,11,3,LV_OPA_80);glyph_box(p,50,80,54,11,3,LV_OPA_80);
  glyph_box(p,45,52,5,43,1,LV_OPA_80);glyph_box(p,104,52,5,43,1,LV_OPA_80);glyph_box(p,62,68,4,12,1,LV_OPA_60);
 }
}
static void room_scene_click_cb(lv_event_t*e){
 int room=(int)(intptr_t)lv_event_get_user_data(e);room_power[room]=!room_power[room];
 lv_obj_t*scene=lv_event_get_target(e);
 lv_obj_set_style_bg_opa(scene,room_power[room]?LV_OPA_COVER:LV_OPA_30,0);
 ui_action_light_select(room);ui_action_light_power(room_power[room]);
}
static void sun_icon(lv_obj_t*p,int x,int y){
 lv_obj_t*core=glyph_box(p,x+6,y+6,13,13,7,LV_OPA_COVER);(void)core;
 glyph_box(p,x+11,y,3,5,1,LV_OPA_COVER);glyph_box(p,x+11,y+20,3,5,1,LV_OPA_COVER);
 glyph_box(p,x,y+11,5,3,1,LV_OPA_COVER);glyph_box(p,x+20,y+11,5,3,1,LV_OPA_COVER);
}
static void temp_icon(lv_obj_t*p,int x,int y){
 glyph_box(p,x+8,y,7,18,4,LV_OPA_COVER);
 glyph_box(p,x+6,y+14,11,11,6,LV_OPA_COVER);
 glyph_box(p,x+10,y+4,3,13,1,LV_OPA_COVER);
}
static void light_slider_cb(lv_event_t*e){
 int data=(int)(intptr_t)lv_event_get_user_data(e);
 int room=data>>1;bool temperature=(data&1)!=0;
 int value=lv_slider_get_value(lv_event_get_target(e));
 ui_action_light_select(room);
 if(temperature)ui_action_light_temperature(value);else ui_action_light_brightness(value);
}
static lv_obj_t *room_slider(lv_obj_t*p,int room,int y,bool temperature){
 lv_obj_t*s=lv_slider_create(p);lv_obj_set_pos(s,151,y);lv_obj_set_size(s,145,42);
 lv_slider_set_range(s,0,100);lv_slider_set_value(s,temperature?58:72,LV_ANIM_OFF);
 lv_obj_set_style_radius(s,21,LV_PART_MAIN);lv_obj_set_style_bg_color(s,lv_color_hex(0x17263A),LV_PART_MAIN);
 lv_obj_set_style_bg_opa(s,LV_OPA_COVER,LV_PART_MAIN);lv_obj_set_style_radius(s,21,LV_PART_INDICATOR);
 lv_obj_set_style_bg_color(s,lv_color_hex(temperature?0xFFB24C:0xFFE09A),LV_PART_INDICATOR);
 lv_obj_set_style_bg_opa(s,LV_OPA_COVER,LV_PART_INDICATOR);lv_obj_set_style_bg_opa(s,LV_OPA_TRANSP,LV_PART_KNOB);
 lv_obj_add_flag(s,LV_OBJ_FLAG_GESTURE_BUBBLE);
 if(temperature)temp_icon(p,160,y+9);else sun_icon(p,158,y+8);
 return s;
}
static lv_obj_t *room_card(lv_obj_t*p,int room,int x){
 lv_obj_t*card=lv_obj_create(p);lv_obj_remove_flag(card,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scrollbar_mode(card,LV_SCROLLBAR_MODE_OFF);
 lv_obj_set_pos(card,x,13);lv_obj_set_size(card,LIGHT_CARD_W,LIGHT_CARD_H);
 lv_obj_set_style_radius(card,24,0);lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0x40597C),0);
 lv_obj_set_style_bg_color(card,lv_color_hex(0x0D1A2D),0);lv_obj_set_style_bg_opa(card,LV_OPA_90,0);lv_obj_set_style_pad_all(card,0,0);
 lv_obj_add_flag(card,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_add_flag(card,LV_OBJ_FLAG_GESTURE_BUBBLE);

 /* Left status / power card is square; tap it to toggle the room. */
 lv_obj_t*scene=lv_obj_create(card);lv_obj_remove_flag(scene,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scrollbar_mode(scene,LV_SCROLLBAR_MODE_OFF);
 lv_obj_set_pos(scene,8,9);lv_obj_set_size(scene,128,128);lv_obj_set_style_radius(scene,20,0);lv_obj_set_style_border_width(scene,0,0);
 room_scene_style(scene,room);lv_obj_set_style_bg_opa(scene,room_power[room]?LV_OPA_COVER:LV_OPA_30,0);
 lv_obj_add_flag(scene,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(scene,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_add_flag(scene,LV_OBJ_FLAG_GESTURE_BUBBLE);
 lv_obj_add_event_cb(scene,room_scene_click_cb,LV_EVENT_CLICKED,(void*)(intptr_t)room);
 lv_obj_t*title=home_text(scene,room_names[room],10,10,&lv_font_montserrat_14,LV_OPA_COVER);
 lv_obj_add_flag(title,LV_OBJ_FLAG_EVENT_BUBBLE);
 room_furniture_glyph(scene,room);

 /* Brightness and color-temperature remain horizontal controls to the right. */
 room_slider(card,room,24,false);
 room_slider(card,room,80,true);
 return card;
}
static void light_render(void){
 lv_obj_clean(s_light);
 int x=12;
 for(int room=light_index;room<LIGHT_ROOM_COUNT;room++){
  room_card(s_light,room,x);x+=LIGHT_CARD_W+LIGHT_CARD_GAP;
 }
}
static bool light_render_pending;
static void light_render_async(void*unused){
 (void)unused;
 light_render_pending=false;
 if(lv_screen_active()==s_light)light_render();
}
static void light_carousel_gesture_cb(lv_event_t*e){
 (void)e;
 if(lv_screen_active()!=s_light)return;
 lv_indev_t*i=lv_indev_active();if(!i)return;
 lv_dir_t d=lv_indev_get_gesture_dir(i);
 if(d==LV_DIR_RIGHT&&press_start.x<=48)return;
 if(d==LV_DIR_LEFT&&light_index<LIGHT_ROOM_COUNT-1)light_index++;
 else if(d==LV_DIR_RIGHT&&light_index>0)light_index--;
 else return;
 ui_action_light_select(light_index);
 if(!light_render_pending){
  light_render_pending=true;
  lv_async_call(light_render_async,NULL);
 }
}
static void light_create(void){
 s_light=lv_obj_create(NULL);base(s_light);gestures(s_light);
 lv_obj_set_style_bg_color(s_light,lv_color_hex(0x07111F),0);lv_obj_set_style_bg_grad_color(s_light,lv_color_hex(0x122A51),0);
 lv_obj_set_style_bg_grad_dir(s_light,LV_GRAD_DIR_HOR,0);light_render();
 lv_obj_add_event_cb(s_light,light_carousel_gesture_cb,LV_EVENT_GESTURE,NULL);
}
static lv_obj_t *remote_key(lv_obj_t*p,int x,int y,int w,int h,uint32_t top,uint32_t bottom,const char *icon,ui_remote_action_t action){
 lv_obj_t *o=lv_obj_create(p);lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
 lv_obj_set_style_radius(o,(w<h?w:h)/3,0);lv_obj_set_style_border_width(o,1,0);
 lv_obj_set_style_border_color(o,lv_color_hex(0x4E6382),0);
 lv_obj_set_style_bg_color(o,lv_color_hex(top),0);lv_obj_set_style_bg_grad_color(o,lv_color_hex(bottom),0);
 lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_VER,0);lv_obj_set_style_shadow_width(o,10,0);
 lv_obj_set_style_shadow_color(o,lv_color_hex(0x315F9B),0);lv_obj_set_style_shadow_opa(o,LV_OPA_20,0);
 lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);
 lv_obj_add_event_cb(o,remote_cb,LV_EVENT_CLICKED,(void*)(intptr_t)action);
 if(icon&&icon[0]){lv_obj_t *i=home_text(o,icon,0,0,&lv_font_montserrat_28,LV_OPA_COVER);lv_obj_center(i);}
 return o;
}
static void remote_touch_cb(lv_event_t*e){
 static lv_point_t p0;lv_indev_t*i=lv_indev_active();if(!i)return;
 if(lv_event_get_code(e)==LV_EVENT_PRESSED){lv_indev_get_point(i,&p0);return;}
 lv_point_t p1;lv_indev_get_point(i,&p1);int dx=p1.x-p0.x,dy=p1.y-p0.y;
 if(dx*dx+dy*dy<24*24){ui_action_remote(UI_REMOTE_OK);return;}
 if(dx*dx>dy*dy)ui_action_remote(dx>0?UI_REMOTE_RIGHT:UI_REMOTE_LEFT);
 else ui_action_remote(dy>0?UI_REMOTE_DOWN:UI_REMOTE_UP);
}
static void remote_create(void){
 s_remote=lv_obj_create(NULL);base(s_remote);gestures(s_remote);
 lv_obj_set_style_bg_color(s_remote,lv_color_hex(0x07111F),0);
 lv_obj_set_style_bg_grad_color(s_remote,lv_color_hex(0x102C52),0);lv_obj_set_style_bg_grad_dir(s_remote,LV_GRAD_DIR_HOR,0);

 /* left stack: power over input; icon artwork is intentionally sideways */
 remote_key(s_remote,12,12,54,68,0xFF3152,0xD90035,"↻",UI_REMOTE_POWER);
 remote_key(s_remote,12,92,54,68,0x26364E,0x152238,"↰",UI_REMOTE_INPUT);

 /* large blank navigation touch surface: swipe = direction, tap = OK */
 lv_obj_t *pad=remote_key(s_remote,78,12,208,148,0x26364E,0x111D2E,"",UI_REMOTE_OK);
 lv_obj_remove_event_cb(pad,remote_cb);lv_obj_add_event_cb(pad,remote_touch_cb,LV_EVENT_PRESSED,NULL);
 lv_obj_add_event_cb(pad,remote_touch_cb,LV_EVENT_RELEASED,NULL);

 /* back / home */
 remote_key(s_remote,298,12,64,68,0x26364E,0x152238,"↙",UI_REMOTE_BACK);
 remote_key(s_remote,298,92,64,68,0x26364E,0x152238,"◀",UI_REMOTE_HOME);

 /* settings + display */
 remote_key(s_remote,374,12,64,68,0x26364E,0x152238,LV_SYMBOL_SETTINGS,UI_REMOTE_SETTINGS);
 remote_key(s_remote,450,12,64,68,0x26364E,0x152238,"▯",UI_REMOTE_DISPLAY);

 /* horizontal volume pill: plus on left, minus on right */
 lv_obj_t *vol=lv_obj_create(s_remote);lv_obj_set_scrollbar_mode(vol,LV_SCROLLBAR_MODE_OFF);lv_obj_set_pos(vol,374,92);lv_obj_set_size(vol,252,68);
 lv_obj_set_style_radius(vol,26,0);lv_obj_set_style_border_width(vol,1,0);lv_obj_set_style_border_color(vol,lv_color_hex(0x4E6382),0);
 lv_obj_set_style_bg_color(vol,lv_color_hex(0x26364E),0);lv_obj_set_style_bg_grad_color(vol,lv_color_hex(0x152238),0);
 lv_obj_set_style_bg_grad_dir(vol,LV_GRAD_DIR_VER,0);
 remote_key(vol,8,7,104,54,0x26364E,0x152238,"+",UI_REMOTE_VOL_UP);
 remote_key(vol,140,7,104,54,0x26364E,0x152238,"|",UI_REMOTE_VOL_DOWN);
}
static lv_obj_t *music_wave[24];static lv_timer_t *music_wave_timer;static bool music_anim_playing=true;static uint8_t music_phase;
static void music_wave_tick(lv_timer_t*t){
 if(!music_anim_playing)return;
 for(int n=0;n<24;n++){int d=n<12?11-n:n-12;int pulse=(music_phase+n*3)%11;int h=5+(11-d)*2+(pulse<5?pulse*2:(10-pulse)*2);if(h>38)h=38;
  lv_obj_set_height(music_wave[n],h);lv_obj_align(music_wave[n],LV_ALIGN_CENTER,(n-12)*10+5,18);}
 music_phase=(music_phase+1)%11;
}
static void music_center_cb(lv_event_t*e){
 music_anim_playing=!music_anim_playing;ui_action_music(UI_MUSIC_PLAY_PAUSE);
 for(int n=0;n<24;n++)lv_obj_set_style_bg_opa(music_wave[n],music_anim_playing?LV_OPA_80:LV_OPA_20,0);
}
static void music_icon_press(lv_event_t*e){lv_obj_set_style_transform_scale(lv_event_get_target(e),230,0);}
static void music_icon_release(lv_event_t*e){lv_obj_set_style_transform_scale(lv_event_get_target(e),256,0);}
static lv_obj_t *music_nav(lv_obj_t*p,const char*icon,int x,ui_music_action_t action){
 lv_obj_t*o=home_text(p,icon,x,61,&lv_font_montserrat_28,LV_OPA_COVER);lv_obj_set_size(o,56,56);
 lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_add_event_cb(o,music_cb,LV_EVENT_CLICKED,(void*)(intptr_t)action);
 lv_obj_add_event_cb(o,music_icon_press,LV_EVENT_PRESSED,NULL);lv_obj_add_event_cb(o,music_icon_release,LV_EVENT_RELEASED,NULL);return o;
}
static void music_create(void){
 s_music=lv_obj_create(NULL);base(s_music);gestures(s_music);
 lv_obj_set_style_bg_color(s_music,lv_color_hex(0x07101E),0);lv_obj_set_style_bg_grad_color(s_music,lv_color_hex(0x172650),0);
 lv_obj_set_style_bg_grad_dir(s_music,LV_GRAD_DIR_HOR,0);

 /* generous left edge remains free for the global back gesture */
 music_nav(s_music,LV_SYMBOL_PREV,82,UI_MUSIC_PREV);music_nav(s_music,LV_SYMBOL_NEXT,502,UI_MUSIC_NEXT);

 /* metadata is optically centered, independent of navigation icons */
 lv_obj_t*title=home_text(s_music,"Midnight Drive",0,12,&lv_font_montserrat_20,LV_OPA_COVER);lv_obj_align(title,LV_ALIGN_TOP_MID,0,12);
 lv_obj_t*artist=home_text(s_music,"The Paper Kites",0,39,&lv_font_montserrat_16,LV_OPA_80);lv_obj_align(artist,LV_ALIGN_TOP_MID,0,39);
 lv_obj_t*album=home_text(s_music,"Twelvefour",0,61,&lv_font_montserrat_14,LV_OPA_60);lv_obj_align(album,LV_ALIGN_TOP_MID,0,61);

 /* decorative pseudo-spectrum: LVGL objects, no bitmap required */
 for(int n=0;n<24;n++){music_wave[n]=lv_obj_create(s_music);lv_obj_set_size(music_wave[n],4,10);
  lv_obj_set_style_radius(music_wave[n],2,0);lv_obj_set_style_border_width(music_wave[n],0,0);
  lv_obj_set_style_bg_color(music_wave[n],lv_color_hex(n%3==0?0xB15CFF:0x477CFF),0);
  lv_obj_set_style_bg_opa(music_wave[n],LV_OPA_80,0);lv_obj_align(music_wave[n],LV_ALIGN_CENTER,(n-12)*10+5,18);}
 music_wave_timer=lv_timer_create(music_wave_tick,90,NULL);

 /* entire central field toggles play/pause; spectrum itself is only a status cue */
 lv_obj_t*tap=lv_obj_create(s_music);lv_obj_set_pos(tap,150,78);lv_obj_set_size(tap,340,46);
 lv_obj_set_style_bg_opa(tap,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(tap,0,0);
 lv_obj_add_flag(tap,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(tap,music_center_cb,LV_EVENT_CLICKED,NULL);

 /* display-only progress bar */
 lv_obj_t*track=lv_obj_create(s_music);lv_obj_set_pos(track,126,143);lv_obj_set_size(track,388,6);
 lv_obj_set_style_radius(track,3,0);lv_obj_set_style_border_width(track,0,0);lv_obj_set_style_bg_color(track,lv_color_hex(0x58617A),0);lv_obj_set_style_bg_opa(track,LV_OPA_60,0);
 lv_obj_t*fill=lv_obj_create(track);lv_obj_set_pos(fill,0,0);lv_obj_set_size(fill,126,6);lv_obj_set_style_radius(fill,3,0);lv_obj_set_style_border_width(fill,0,0);lv_obj_set_style_bg_color(fill,lv_color_hex(0x6E7DFF),0);
 lv_obj_t*dot=lv_obj_create(track);lv_obj_set_pos(dot,119,-4);lv_obj_set_size(dot,14,14);lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(dot,0,0);lv_obj_set_style_bg_color(dot,lv_color_hex(0xFFFFFF),0);
 lv_obj_t*l=home_text(s_music,"01:28",78,137,&lv_font_montserrat_14,LV_OPA_70);(void)l;
 home_text(s_music,"04:32",528,137,&lv_font_montserrat_14,LV_OPA_70);
}
static void devices_create(void){s_devices=lv_obj_create(NULL);base(s_devices);gestures(s_devices);label(s_devices,"DEVICES",24,14,14);lv_obj_t*c=card(s_devices,176,36,288,112);lv_obj_t*i=label(c,LV_SYMBOL_HOME,120,10,28);lv_obj_set_style_text_color(i,lv_color_hex(BLUE),0);label(c,"No devices yet",82,48,16);lv_obj_t*t=label(c,"Reserved for HA / robot devices",32,76,14);lv_obj_set_style_text_color(t,lv_color_hex(MUTED),0);}

void ui_init(void){home_create();apps_create();settings_create();light_create();remote_create();music_create();devices_create();return_screen=s_home;lv_screen_load(s_home);}
void ui_show_home(void){load(s_home);}void ui_show_functions(void){load(s_apps);}void ui_show_settings(void){if(lv_screen_active()!=s_home)return;return_screen=s_home;load(s_settings);}
