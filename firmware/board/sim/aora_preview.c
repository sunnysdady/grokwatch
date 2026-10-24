// Off-device preview and lifecycle regression check. Never opens a serial port.
#include "gw_faces.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
LV_FONT_DECLARE(gz_20)
const lv_font_t* gw_f20(void){ return &gz_20; }
const lv_font_t* gw_f21(void){ return &gz_20; }
static uint32_t now=1000, flushes;
static uint16_t pixels[466*466], buffer[466*466];
static uint32_t get_tick(void){ return now; }
static void flush(lv_display_t* d,const lv_area_t* a,uint8_t* src){
    assert(a->x1>=0 && a->y1>=0 && a->x2<466 && a->y2<466);
    for(int y=a->y1;y<=a->y2;y++)
        memcpy(pixels+y*466+a->x1,src+(y-a->y1)*(a->x2-a->x1+1)*2,(a->x2-a->x1+1)*2);
    flushes++; lv_display_flush_ready(d);
}
static void step(int ms){ for(int i=0;i<ms;i+=10){ now+=10; lv_timer_handler(); } }
static void shot(const char* dir,const char* name){
    char path[1024]; snprintf(path,sizeof path,"%s/%s.ppm",dir,name);
    FILE* f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n466 466\n255\n");
    for(int y=0;y<466;y++) for(int x=0;x<466;x++){
        uint16_t c=pixels[y*466+x]; int outside=(x-233)*(x-233)+(y-233)*(y-233)>233*233;
        fputc(outside?24:((c>>11)&31)*255/31,f);
        fputc(outside?24:((c>>5)&63)*255/63,f); fputc(outside?24:(c&31)*255/31,f);
    }
    fclose(f);
}
int main(int argc,char** argv){
    assert(argc==2); lv_init(); lv_tick_set_cb(get_tick);
    lv_display_t* d=lv_display_create(466,466); lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,buffer,NULL,sizeof buffer,LV_DISPLAY_RENDER_MODE_PARTIAL); lv_display_set_flush_cb(d,flush);
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_black(),0);
    gw_grok_state(4); gw_grok_create(lv_screen_active()); gw_grok_visible(true);
    const char* names[]={"idle","listening","thinking","speaking","offline"};
    for(int state=0;state<5;state++){
        gw_grok_state(state); gw_grok_level(80);
        gw_grok_text("user",state==1?"明天适合骑车吗？":"");
        gw_grok_text("assistant",state==3?"明天晴天，出发前记得带水。":"");
        step(1200); shot(argv[1],names[state]);
    }
    gw_grok_set_night(true);
    for(int state=0;state<5;state++){ char nm[32]; gw_grok_state(state); gw_grok_level(80);
        gw_grok_text("user",state==1?"明天适合骑车吗？":""); gw_grok_text("assistant",state==3?"明天晴天，出发前记得带水。":"");
        step(1200); snprintf(nm,sizeof nm,"n_%s",names[state]); shot(argv[1],nm); }
    gw_grok_set_night(false);
    { // host-only render cost of one animated avatar frame (NOT a device number)
      struct timespec a,b; double tot=0,mx=0; gw_grok_state(3);
      for(int i=0;i<200;i++){ now+=33; clock_gettime(CLOCK_MONOTONIC,&a); lv_timer_handler(); clock_gettime(CLOCK_MONOTONIC,&b);
        double ms=(b.tv_sec-a.tv_sec)*1e3+(b.tv_nsec-a.tv_nsec)/1e6; tot+=ms; if(ms>mx) mx=ms; }
      printf("PERF host speaking frame (ASan -O1): avg %.3f ms max %.3f ms\n",tot/200,mx); }
    gw_grok_state(0); gw_grok_text("user",""); gw_grok_text("assistant",""); step(900);
    for(int frame=0;frame<48;frame++){ char name[40]; snprintf(name,sizeof name,"motion_%02d",frame); step(40); shot(argv[1],name); }
    lv_mem_monitor_t before,after; lv_mem_monitor(&before);
    for(int cycle=0;cycle<50;cycle++) for(int state=0;state<5;state++){
        gw_grok_state(state); gw_grok_level(cycle*2); step(400);
        gw_grok_visible(false); step(100); uint32_t n=flushes; step(100); assert(flushes==n);
        gw_grok_visible(true);
    }
    // Cover every pool entry and intermediate shape, including long intervals.
    for(int state=0;state<5;state++){ gw_grok_state(state); step(70000); }
    gw_grok_state(-1); gw_grok_state(5); step(100);
    lv_mem_monitor(&after); assert(before.free_size<=after.free_size+4096);
    printf("PASS: five states, 250 hide/show cycles, all contour pools, no hidden redraws; heap delta=%ld bytes\n",(long)before.free_size-(long)after.free_size);
    lv_deinit(); return 0;
}
