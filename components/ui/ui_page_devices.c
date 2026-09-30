#include "ui_page_devices.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define W 640
#define H 172
#define PAGE_N 4
#define VIEW_N 2
#define BG lv_color_hex(0x000000)
#define BTN lv_color_hex(0x111824)
#define BTN2 lv_color_hex(0x171F2D)
#define BR lv_color_hex(0x526581)
#define BR_ON lv_color_hex(0x7897C8)
#define PRESS lv_color_hex(0x24344D)
#define CHECK lv_color_hex(0x22324A)
#define CARD lv_color_hex(0x0B121D)
#define CARD2 lv_color_hex(0x111D2D)
#define FG lv_color_hex(0xEAF2FF)
#define DIM lv_color_hex(0x536071)
#define DIS_BG lv_color_hex(0x090D14)
#define DIS_BR lv_color_hex(0x263142)
#define POWER lv_color_hex(0xFF604F)
#define TMIN 32
#define TMAX 62
#define TSTEP 12
#define FMIN 1
#define FMAX 7
#define RI_POWER "\xEF\x84\xA6"
#define RI_COOL "\xEF\x94\x92"
#define RI_HEAT "\xEF\x86\xBF"
#define RI_FAN "\xEF\x8B\x8A"
#define RI_DRY "\xEE\xB1\xAA"
#define RI_SWING "\xEE\xA9\xA2"
#define RI_THERM "\xEF\x87\xB2"
#if defined(UI_DEVICES_HAS_FONTS)
LV_FONT_DECLARE(ui_font_source_han_devices_16);
LV_FONT_DECLARE(ui_font_remix_devices_28);
LV_FONT_DECLARE(ui_font_remix_devices_56);
#define TF (&ui_font_source_han_devices_16)
#define IF (&ui_font_remix_devices_28)
#define ILF (&ui_font_remix_devices_56)
#else
#define TF LV_FONT_DEFAULT
#define IF LV_FONT_DEFAULT
#define ILF LV_FONT_DEFAULT
#endif
typedef struct{lv_obj_t*seg[7];} digit_t;
typedef enum{CENTER_TEMP,CENTER_MODE,CENTER_FAN} center_t;
typedef struct{
 lv_obj_t*power;lv_obj_t*power_i;lv_obj_t*mode;lv_obj_t*mode_i;lv_obj_t*swing;lv_obj_t*swing_i;lv_obj_t*fan;lv_obj_t*fan_i;
 lv_obj_t*card;lv_obj_t*temp;digit_t d[3];lv_obj_t*dot;lv_obj_t*degree;lv_obj_t*off_i;lv_obj_t*temp_g;
 lv_obj_t*mode_p;lv_obj_t*mode_b[4];lv_obj_t*fan_p;lv_obj_t*fan_v;lv_obj_t*slider;lv_obj_t*auto_b;lv_obj_t*feat[4];
} view_t;
static const char*page_names[4]={"AIR CONDITIONER","CURTAIN","BATH HEATER","DRYING RACK"};
static const char*feat_names[4]={"睡眠","ECO","干燥","辅热"};
static const char*mode_icons[4]={RI_COOL,RI_HEAT,RI_FAN,RI_DRY};
static const uint8_t masks[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
static lv_obj_t*root,*pager;static bool wrapping,updating;static ui_devices_activity_cb_t acb;static void*aud;
static view_t views[VIEW_N];static size_t view_n;static bool power_on=true,swing_on,features[4];static uint8_t mode_idx;static int temp2=48,fan_speed=4;static bool fan_auto;static center_t center=CENTER_TEMP;static int32_t temp_y;
static void activity(void){if(acb)acb(aud);}static void hidden(lv_obj_t*o,bool h){if(!o)return;if(h)lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);}static void enabled(lv_obj_t*o,bool e){if(!o)return;if(e)lv_obj_remove_state(o,LV_STATE_DISABLED);else lv_obj_add_state(o,LV_STATE_DISABLED);}static void checked(lv_obj_t*o,bool c){if(!o)return;if(c)lv_obj_add_state(o,LV_STATE_CHECKED);else lv_obj_remove_state(o,LV_STATE_CHECKED);}
static void style_btn(lv_obj_t*o,int r){lv_obj_set_style_radius(o,r,0);lv_obj_set_style_bg_color(o,BTN,0);lv_obj_set_style_bg_grad_color(o,BTN2,0);lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_VER,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,BR,0);lv_obj_set_style_border_opa(o,LV_OPA_70,0);lv_obj_set_style_bg_color(o,PRESS,LV_STATE_PRESSED);lv_obj_set_style_bg_grad_color(o,PRESS,LV_STATE_PRESSED);lv_obj_set_style_border_color(o,BR_ON,LV_STATE_PRESSED);lv_obj_set_style_bg_color(o,CHECK,LV_STATE_CHECKED);lv_obj_set_style_bg_grad_color(o,PRESS,LV_STATE_CHECKED);lv_obj_set_style_border_color(o,BR_ON,LV_STATE_CHECKED);lv_obj_set_style_bg_color(o,DIS_BG,LV_STATE_DISABLED);lv_obj_set_style_bg_grad_color(o,DIS_BG,LV_STATE_DISABLED);lv_obj_set_style_border_color(o,DIS_BR,LV_STATE_DISABLED);lv_obj_set_style_text_color(o,DIM,LV_STATE_DISABLED);}
static lv_obj_t*ibtn(lv_obj_t*p,const char*g,int x,int y,int w,int h,bool ck,lv_obj_t**io){lv_obj_t*b=lv_obj_create(p);lv_obj_remove_style_all(b);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);style_btn(b,18);lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);lv_obj_clear_flag(b,LV_OBJ_FLAG_SCROLLABLE);if(ck)lv_obj_add_flag(b,LV_OBJ_FLAG_CHECKABLE);lv_obj_t*i=lv_label_create(b);lv_label_set_text(i,g);lv_obj_set_style_text_font(i,IF,0);lv_obj_set_style_text_color(i,FG,0);lv_obj_center(i);lv_obj_clear_flag(i,LV_OBJ_FLAG_CLICKABLE);if(io)*io=i;return b;}
static lv_obj_t*tbtn(lv_obj_t*p,const char*t,int x,int y,int w,int h){lv_obj_t*b=lv_obj_create(p);lv_obj_remove_style_all(b);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);style_btn(b,18);lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CHECKABLE);lv_obj_clear_flag(b,LV_OBJ_FLAG_SCROLLABLE);lv_obj_t*l=lv_label_create(b);lv_label_set_text(l,t);lv_obj_set_style_text_font(l,TF,0);lv_obj_set_style_text_color(l,FG,0);lv_obj_center(l);lv_obj_clear_flag(l,LV_OBJ_FLAG_CLICKABLE);return b;}
static lv_obj_t*seg(lv_obj_t*p,int x,int y,int w,int h){lv_obj_t*o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_radius(o,3,0);lv_obj_set_style_bg_color(o,FG,0);lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;}
static void build_digit(lv_obj_t*p,digit_t*d,int x){lv_obj_t*h=lv_obj_create(p);lv_obj_remove_style_all(h);lv_obj_set_pos(h,x,0);lv_obj_set_size(h,42,98);lv_obj_clear_flag(h,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);d->seg[0]=seg(h,9,2,24,6);d->seg[1]=seg(h,34,10,6,35);d->seg[2]=seg(h,34,53,6,35);d->seg[3]=seg(h,9,90,24,6);d->seg[4]=seg(h,2,53,6,35);d->seg[5]=seg(h,2,10,6,35);d->seg[6]=seg(h,9,46,24,6);}
static void set_digit(digit_t*d,uint8_t n,lv_color_t c){uint8_t m=n<10?masks[n]:0;for(int i=0;i<7;i++){lv_obj_set_style_bg_color(d->seg[i],c,0);lv_obj_set_style_bg_opa(d->seg[i],m&(1u<<i)?LV_OPA_COVER:LV_OPA_TRANSP,0);}}
static void temp_refresh(view_t*v){int whole=temp2/2;lv_color_t c=power_on?FG:DIM;set_digit(&v->d[0],whole/10,c);set_digit(&v->d[1],whole%10,c);set_digit(&v->d[2],(temp2&1)?5:0,c);lv_obj_set_style_bg_color(v->dot,c,0);lv_obj_set_style_border_color(v->degree,c,0);}
static void refresh(void){char ft[8];snprintf(ft,sizeof(ft),"%d",fan_speed);updating=true;for(size_t i=0;i<view_n;i++){view_t*v=&views[i];bool sm=power_on&&center==CENTER_MODE,sf=power_on&&center==CENTER_FAN,st=!sm&&!sf;checked(v->power,power_on);checked(v->mode,sm);checked(v->swing,swing_on);checked(v->fan,sf);checked(v->auto_b,fan_auto);for(int j=0;j<4;j++){checked(v->feat[j],features[j]);checked(v->mode_b[j],mode_idx==(uint8_t)j);enabled(v->feat[j],power_on);enabled(v->mode_b[j],sm);}enabled(v->mode,power_on);enabled(v->swing,power_on);enabled(v->fan,power_on);enabled(v->temp_g,power_on&&st);enabled(v->slider,sf);enabled(v->auto_b,sf);lv_label_set_text(v->mode_i,mode_icons[mode_idx]);lv_slider_set_value(v->slider,fan_speed,LV_ANIM_OFF);lv_obj_set_style_text_color(v->power_i,POWER,0);lv_obj_set_style_text_color(v->mode_i,power_on?FG:DIM,0);lv_obj_set_style_text_color(v->swing_i,power_on?FG:DIM,0);lv_obj_set_style_text_color(v->fan_i,power_on?FG:DIM,0);temp_refresh(v);lv_label_set_text(v->fan_v,fan_auto?"AUTO":ft);hidden(v->temp,!st||!power_on);hidden(v->off_i,!st||power_on);hidden(v->temp_g,!st||!power_on);hidden(v->card,st);hidden(v->mode_p,!sm);hidden(v->fan_p,!sf);}updating=false;}
static void power_cb(lv_event_t*e){if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;power_on=!power_on;if(!power_on)center=CENTER_TEMP;refresh();activity();}
static void mode_cb(lv_event_t*e){if(lv_event_get_code(e)!=LV_EVENT_CLICKED||!power_on)return;center=center==CENTER_MODE?CENTER_TEMP:CENTER_MODE;refresh();activity();}
static void swing_cb(lv_event_t*e){if(lv_event_get_code(e)!=LV_EVENT_CLICKED||!power_on)return;swing_on=!swing_on;refresh();activity();}
static void fan_btn_cb(lv_event_t*e){if(lv_event_get_code(e)!=LV_EVENT_CLICKED||!power_on)return;center=center==CENTER_FAN?CENTER_TEMP:CENTER_FAN;refresh();activity();}
static void feat_cb(lv_event_t*e){if(lv_event_get_code(e)!=LV_EVENT_CLICKED||!power_on)return;intptr_t n=(intptr_t)lv_event_get_user_data(e);if(n<0||n>3)return;features[n]=!features[n];refresh();activity();}
static void mode_opt_cb(lv_event_t*e){if(lv_event_get_code(e)!=LV_EVENT_CLICKED||!power_on)return;intptr_t n=(intptr_t)lv_event_get_user_data(e);if(n<0||n>3)return;mode_idx=(uint8_t)n;center=CENTER_TEMP;refresh();activity();}
static void temp_cb(lv_event_t*e){if(!power_on||center!=CENTER_TEMP)return;lv_indev_t*i=lv_event_get_indev(e);if(!i)return;lv_event_code_t c=lv_event_get_code(e);lv_point_t p;lv_indev_get_point(i,&p);if(c==LV_EVENT_PRESSED){temp_y=p.y;activity();return;}if(c==LV_EVENT_PRESSING){int32_t dy=p.y-temp_y;bool ch=false;while(dy<=-TSTEP&&temp2<TMAX){temp2++;temp_y-=TSTEP;dy+=TSTEP;ch=true;}while(dy>=TSTEP&&temp2>TMIN){temp2--;temp_y+=TSTEP;dy-=TSTEP;ch=true;}if(ch)refresh();activity();return;}if(c==LV_EVENT_RELEASED||c==LV_EVENT_PRESS_LOST)activity();}
static void auto_cb(lv_event_t*e){if(lv_event_get_code(e)!=LV_EVENT_LONG_PRESSED||!power_on||center!=CENTER_FAN)return;fan_auto=true;refresh();activity();}
static void slider_cb(lv_event_t*e){if(!power_on||updating||center!=CENTER_FAN)return;lv_event_code_t c=lv_event_get_code(e);if(c==LV_EVENT_PRESSED||c==LV_EVENT_PRESSING||c==LV_EVENT_VALUE_CHANGED){fan_speed=lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));fan_auto=false;refresh();}if(c==LV_EVENT_PRESSED||c==LV_EVENT_PRESSING||c==LV_EVENT_VALUE_CHANGED||c==LV_EVENT_RELEASED||c==LV_EVENT_PRESS_LOST)activity();}
static void normalize(void){if(!pager||wrapping)return;int32_t x=lv_obj_get_scroll_x(pager);wrapping=true;if(x<=0)lv_obj_scroll_to_x(pager,PAGE_N*W,LV_ANIM_OFF);else if(x>=(PAGE_N+1)*W)lv_obj_scroll_to_x(pager,W,LV_ANIM_OFF);wrapping=false;}
static void pager_cb(lv_event_t*e){lv_event_code_t c=lv_event_get_code(e);if(c==LV_EVENT_PRESSED||c==LV_EVENT_PRESSING||c==LV_EVENT_SCROLL_BEGIN||c==LV_EVENT_SCROLL||c==LV_EVENT_SCROLL_END||c==LV_EVENT_RELEASED)activity();if(c==LV_EVENT_SCROLL_BEGIN&&power_on&&center!=CENTER_TEMP){center=CENTER_TEMP;refresh();}if(c==LV_EVENT_SCROLL_END)normalize();}
static lv_obj_t*page(lv_obj_t*p){lv_obj_t*o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_size(o,W,H);lv_obj_set_style_bg_color(o,BG,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;}
static void placeholder(lv_obj_t*p,const char*t){lv_obj_t*o=page(p);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);lv_obj_t*l=lv_label_create(o);lv_label_set_text(l,t);lv_obj_set_style_text_color(l,FG,0);lv_obj_align(l,LV_ALIGN_CENTER,0,0);lv_obj_clear_flag(l,LV_OBJ_FLAG_CLICKABLE);}
static void card_style(lv_obj_t*o){lv_obj_set_style_radius(o,24,0);lv_obj_set_style_bg_color(o,CARD,0);lv_obj_set_style_bg_grad_color(o,CARD2,0);lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_VER,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,BR,0);lv_obj_set_style_border_opa(o,LV_OPA_60,0);}
static void build_aircon(lv_obj_t*p){if(view_n>=VIEW_N)return;view_t*v=&views[view_n++];lv_obj_t*pg=page(p);const int lx[2]={68,148},rx[2]={420,500},yy[2]={33,97};const int bw=72,bh=42;v->power=ibtn(pg,RI_POWER,lx[0],yy[0],bw,bh,false,&v->power_i);lv_obj_set_style_text_color(v->power_i,POWER,0);lv_obj_add_event_cb(v->power,power_cb,LV_EVENT_CLICKED,NULL);v->mode=ibtn(pg,mode_icons[mode_idx],lx[1],yy[0],bw,bh,false,&v->mode_i);lv_obj_add_event_cb(v->mode,mode_cb,LV_EVENT_CLICKED,NULL);v->swing=ibtn(pg,RI_SWING,lx[0],yy[1],bw,bh,true,&v->swing_i);lv_obj_add_event_cb(v->swing,swing_cb,LV_EVENT_CLICKED,NULL);v->fan=ibtn(pg,RI_FAN,lx[1],yy[1],bw,bh,false,&v->fan_i);lv_obj_add_event_cb(v->fan,fan_btn_cb,LV_EVENT_CLICKED,NULL);
 v->temp=lv_obj_create(pg);lv_obj_remove_style_all(v->temp);lv_obj_set_pos(v->temp,231,37);lv_obj_set_size(v->temp,178,100);lv_obj_clear_flag(v->temp,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);build_digit(v->temp,&v->d[0],0);build_digit(v->temp,&v->d[1],45);build_digit(v->temp,&v->d[2],103);v->dot=lv_obj_create(v->temp);lv_obj_remove_style_all(v->dot);lv_obj_set_pos(v->dot,92,86);lv_obj_set_size(v->dot,8,8);lv_obj_set_style_radius(v->dot,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_opa(v->dot,LV_OPA_COVER,0);v->degree=lv_obj_create(v->temp);lv_obj_remove_style_all(v->degree);lv_obj_set_pos(v->degree,151,2);lv_obj_set_size(v->degree,22,22);lv_obj_set_style_radius(v->degree,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_opa(v->degree,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(v->degree,5,0);lv_obj_set_style_border_opa(v->degree,LV_OPA_COVER,0);v->off_i=lv_label_create(pg);lv_label_set_text(v->off_i,RI_THERM);lv_obj_set_style_text_font(v->off_i,ILF,0);lv_obj_set_style_text_color(v->off_i,DIM,0);lv_obj_set_width(v->off_i,200);lv_obj_set_style_text_align(v->off_i,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_pos(v->off_i,220,58);v->temp_g=lv_obj_create(pg);lv_obj_remove_style_all(v->temp_g);lv_obj_set_pos(v->temp_g,222,20);lv_obj_set_size(v->temp_g,196,134);lv_obj_set_style_bg_opa(v->temp_g,LV_OPA_TRANSP,0);lv_obj_add_flag(v->temp_g,LV_OBJ_FLAG_CLICKABLE);lv_obj_clear_flag(v->temp_g,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(v->temp_g,temp_cb,LV_EVENT_PRESSED,NULL);lv_obj_add_event_cb(v->temp_g,temp_cb,LV_EVENT_PRESSING,NULL);lv_obj_add_event_cb(v->temp_g,temp_cb,LV_EVENT_RELEASED,NULL);lv_obj_add_event_cb(v->temp_g,temp_cb,LV_EVENT_PRESS_LOST,NULL);
 v->card=lv_obj_create(pg);lv_obj_remove_style_all(v->card);lv_obj_set_pos(v->card,228,25);lv_obj_set_size(v->card,184,122);card_style(v->card);lv_obj_clear_flag(v->card,LV_OBJ_FLAG_SCROLLABLE);v->mode_p=lv_obj_create(v->card);lv_obj_remove_style_all(v->mode_p);lv_obj_set_pos(v->mode_p,9,12);lv_obj_set_size(v->mode_p,166,98);lv_obj_clear_flag(v->mode_p,LV_OBJ_FLAG_SCROLLABLE);for(int i=0;i<4;i++){int c=i&1,r=i>>1;v->mode_b[i]=ibtn(v->mode_p,mode_icons[i],c*88,r*56,78,42,false,NULL);lv_obj_set_style_radius(v->mode_b[i],14,0);lv_obj_add_event_cb(v->mode_b[i],mode_opt_cb,LV_EVENT_CLICKED,(void*)(intptr_t)i);}v->fan_p=lv_obj_create(v->card);lv_obj_remove_style_all(v->fan_p);lv_obj_set_pos(v->fan_p,8,11);lv_obj_set_size(v->fan_p,168,100);lv_obj_clear_flag(v->fan_p,LV_OBJ_FLAG_SCROLLABLE);v->fan_v=lv_label_create(v->fan_p);lv_obj_set_width(v->fan_v,168);lv_obj_set_style_text_align(v->fan_v,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_font(v->fan_v,TF,0);lv_obj_set_style_text_color(v->fan_v,FG,0);v->slider=lv_slider_create(v->fan_p);lv_obj_set_pos(v->slider,10,34);lv_obj_set_size(v->slider,148,8);lv_slider_set_range(v->slider,FMIN,FMAX);lv_obj_set_style_radius(v->slider,4,LV_PART_MAIN);lv_obj_set_style_bg_color(v->slider,BR,LV_PART_MAIN);lv_obj_set_style_bg_opa(v->slider,LV_OPA_50,LV_PART_MAIN);lv_obj_set_style_bg_color(v->slider,FG,LV_PART_INDICATOR);lv_obj_set_style_bg_color(v->slider,FG,LV_PART_KNOB);lv_obj_set_style_pad_all(v->slider,4,LV_PART_KNOB);lv_obj_add_event_cb(v->slider,slider_cb,LV_EVENT_ALL,NULL);v->auto_b=tbtn(v->fan_p,"AUTO",44,60,80,28);lv_obj_clear_flag(v->auto_b,LV_OBJ_FLAG_CHECKABLE);lv_obj_add_event_cb(v->auto_b,auto_cb,LV_EVENT_LONG_PRESSED,NULL);for(int i=0;i<4;i++){int c=i&1,r=i>>1;v->feat[i]=tbtn(pg,feat_names[i],rx[c],yy[r],bw,bh);lv_obj_add_event_cb(v->feat[i],feat_cb,LV_EVENT_CLICKED,(void*)(intptr_t)i);}refresh();}
lv_obj_t*ui_page_devices_build(lv_obj_t*parent,ui_devices_activity_cb_t cb,void*ud){acb=cb;aud=ud;wrapping=false;updating=false;view_n=0;root=lv_obj_create(parent);lv_obj_remove_style_all(root);lv_obj_set_size(root,W,H);lv_obj_set_style_bg_color(root,BG,0);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);pager=lv_obj_create(root);lv_obj_remove_style_all(pager);lv_obj_set_size(pager,W,H);lv_obj_set_style_bg_color(pager,BG,0);lv_obj_set_style_bg_opa(pager,LV_OPA_COVER,0);lv_obj_set_style_pad_all(pager,0,0);lv_obj_set_scroll_dir(pager,LV_DIR_HOR);lv_obj_set_scroll_snap_x(pager,LV_SCROLL_SNAP_CENTER);lv_obj_add_flag(pager,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_ONE);lv_obj_set_flex_flow(pager,LV_FLEX_FLOW_ROW);lv_obj_set_flex_align(pager,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);placeholder(pager,page_names[3]);build_aircon(pager);placeholder(pager,page_names[1]);placeholder(pager,page_names[2]);placeholder(pager,page_names[3]);build_aircon(pager);lv_obj_add_event_cb(pager,pager_cb,LV_EVENT_ALL,NULL);lv_obj_scroll_to_x(pager,W,LV_ANIM_OFF);return root;}
void ui_page_devices_stop(void){root=NULL;pager=NULL;wrapping=false;updating=false;view_n=0;acb=NULL;aud=NULL;}
