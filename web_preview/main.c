#include "lvgl.h"
#include "ui_manager.h"
#include <SDL.h>
#include <emscripten.h>
#include <stdbool.h>
#include <stdint.h>

#define W 640
#define H 172
#define PIXELS (W * H)

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static uint32_t fb[PIXELS];
static int mx,my;
static bool down;
static uint32_t last;

static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p)
{
    const lv_color16_t *src=(const lv_color16_t*)p;
    int aw=a->x2-a->x1+1;
    int ah=a->y2-a->y1+1;

    for(int yy=0;yy<ah;yy++) {
        for(int xx=0;xx<aw;xx++) {
            int x=a->x1+xx;
            int y=a->y1+yy;
            if(x<0||x>=W||y<0||y>=H) continue;
            lv_color16_t v=src[yy*aw+xx];
            uint8_t r=(v.red*255)/31;
            uint8_t g=(v.green*255)/63;
            uint8_t b=(v.blue*255)/31;
            fb[y*W+x]=0xff000000u|(r<<16)|(g<<8)|b;
        }
    }
    lv_display_flush_ready(d);
}

static void pointer(lv_indev_t*i,lv_indev_data_t*d)
{
    (void)i;
    d->point.x=mx;
    d->point.y=my;
    d->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}

/* Browser pointer input is bridged directly from shell.html.
   Do not route pointer events through SDL in Web Preview: when the canvas is
   CSS-scaled, browser/SDL synthetic mouse+touch events can duplicate/flood
   the queue and do not provide a stable 640x172 coordinate space. */
EMSCRIPTEN_KEEPALIVE
void web_pointer(int x,int y,int pressed)
{
    if(x<0)x=0; else if(x>=W)x=W-1;
    if(y<0)y=0; else if(y>=H)y=H-1;
    mx=x;
    my=y;
    down=pressed!=0;
}

static void frame(void)
{
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
        /* Pointer events are intentionally ignored; shell.html owns them. */
        if(e.type==SDL_QUIT) return;
    }

    uint32_t now=SDL_GetTicks();
    lv_tick_inc(now-last);
    last=now;
    lv_timer_handler();

    SDL_UpdateTexture(tex,NULL,fb,W*4);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren,tex,NULL,NULL);
    SDL_RenderPresent(ren);
}

int main(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    /* Keep SDL from enqueueing browser-generated pointer traffic. */
    SDL_EventState(SDL_MOUSEMOTION,SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONDOWN,SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONUP,SDL_IGNORE);
    SDL_EventState(SDL_FINGERMOTION,SDL_IGNORE);
    SDL_EventState(SDL_FINGERDOWN,SDL_IGNORE);
    SDL_EventState(SDL_FINGERUP,SDL_IGNORE);

    win=SDL_CreateWindow("AI Robot Web Preview",0,0,W,H,0);
    ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED);
    tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,W,H);

    lv_init();
    lv_display_t*disp=lv_display_create(W,H);
    static uint8_t b1[W*40*2],b2[W*40*2];
    lv_display_set_buffers(disp,b1,b2,sizeof(b1),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp,flush);

    lv_indev_t*i=lv_indev_create();
    lv_indev_set_type(i,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(i,pointer);

    ui_init();
    last=SDL_GetTicks();
    emscripten_set_main_loop(frame,0,1);
    return 0;
}
