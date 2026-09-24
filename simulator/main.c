#include "lvgl.h"
#include "ui_manager.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static SDL_Window *win; static SDL_Renderer *ren; static SDL_Texture *tex;
static uint32_t fb[320*172]; static int mx,my; static bool down;

static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){
 const lv_color16_t*c=(const lv_color16_t*)p;
 for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++){lv_color16_t v=*c++;uint8_t r=(v.red*255)/31,g=(v.green*255)/63,b=(v.blue*255)/31;fb[y*320+x]=0xff000000u|(r<<16)|(g<<8)|b;}
 lv_display_flush_ready(d);
}
static void pointer(lv_indev_t*i,lv_indev_data_t*d){(void)i;d->point.x=mx;d->point.y=my;d->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
int main(void){
 SDL_Init(SDL_INIT_VIDEO);win=SDL_CreateWindow("ESP32 AI Robot UI - 320x172",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,516,0);ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED);tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,172);
 lv_init();lv_display_t*d=lv_display_create(320,172);static uint8_t b1[320*40*2],b2[320*40*2];lv_display_set_buffers(d,b1,b2,sizeof(b1),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 lv_indev_t*i=lv_indev_create();lv_indev_set_type(i,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(i,pointer);ui_init();
 uint32_t last=SDL_GetTicks();bool run=true;while(run){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)run=false;else if(e.type==SDL_MOUSEMOTION){mx=e.motion.x/3;my=e.motion.y/3;}else if(e.type==SDL_MOUSEBUTTONDOWN)down=true;else if(e.type==SDL_MOUSEBUTTONUP)down=false;}uint32_t now=SDL_GetTicks();lv_tick_inc(now-last);last=now;lv_timer_handler();SDL_UpdateTexture(tex,NULL,fb,320*4);SDL_RenderClear(ren);SDL_RenderCopy(ren,tex,NULL,NULL);SDL_RenderPresent(ren);SDL_Delay(5);}SDL_Quit();return 0;
}
