#include "lvgl.h"
#include "ui_manager.h"
#include <SDL.h>
#include <emscripten.h>
#include <stdbool.h>
#include <stdint.h>
#define LAND_W 640
#define LAND_H 172
#define PIXELS (LAND_W * LAND_H)
static SDL_Window *win; static SDL_Renderer *ren; static SDL_Texture *tex; static lv_display_t *disp;
static uint32_t fb[PIXELS]; static int mx,my,ui_w=LAND_W,ui_h=LAND_H; static bool down; static uint32_t last;
static void recreate_texture(void){if(tex)SDL_DestroyTexture(tex);tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,ui_w,ui_h);SDL_SetWindowSize(win,ui_w,ui_h);}
void ui_action_orientation(bool portrait){int nw=portrait?172:640,nh=portrait?640:172;if(nw==ui_w&&nh==ui_h)return;ui_w=nw;ui_h=nh;mx=0;my=0;lv_display_set_resolution(disp,ui_w,ui_h);recreate_texture();EM_ASM({if(window.robotPreviewResize)window.robotPreviewResize($0,$1);},ui_w,ui_h);}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){const lv_color16_t*c=(const lv_color16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++){lv_color16_t v=*c++;uint8_t r=(v.red*255)/31,g=(v.green*255)/63,b=(v.blue*255)/31;fb[y*ui_w+x]=0xff000000u|(r<<16)|(g<<8)|b;}lv_display_flush_ready(d);}
static void pointer(lv_indev_t*i,lv_indev_data_t*d){(void)i;d->point.x=mx;d->point.y=my;d->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void frame(void){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_MOUSEMOTION){mx=e.motion.x;my=e.motion.y;}else if(e.type==SDL_MOUSEBUTTONDOWN){mx=e.button.x;my=e.button.y;down=true;}else if(e.type==SDL_MOUSEBUTTONUP){mx=e.button.x;my=e.button.y;down=false;}else if(e.type==SDL_FINGERMOTION||e.type==SDL_FINGERDOWN||e.type==SDL_FINGERUP){mx=(int)(e.tfinger.x*ui_w);my=(int)(e.tfinger.y*ui_h);down=e.type!=SDL_FINGERUP;}}uint32_t now=SDL_GetTicks();lv_tick_inc(now-last);last=now;lv_timer_handler();SDL_UpdateTexture(tex,NULL,fb,ui_w*4);SDL_RenderClear(ren);SDL_RenderCopy(ren,tex,NULL,NULL);SDL_RenderPresent(ren);}
int main(void){SDL_Init(SDL_INIT_VIDEO);win=SDL_CreateWindow("AI Robot Web Preview",0,0,LAND_W,LAND_H,0);ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED);recreate_texture();lv_init();disp=lv_display_create(LAND_W,LAND_H);static uint8_t b1[LAND_W*40*2],b2[LAND_W*40*2];lv_display_set_buffers(disp,b1,b2,sizeof(b1),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(disp,flush);lv_indev_t*i=lv_indev_create();lv_indev_set_type(i,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(i,pointer);ui_init();last=SDL_GetTicks();emscripten_set_main_loop(frame,0,1);return 0;}
