// P0 repro (host): page font F20 = copy of gz_20 whose .fallback points at a heap-owned lv_font_t
// (like LvglBuiltInFont, which COPIES the builtin font into a shared_ptr-owned object).
// SetTextFont() frees that object; the next layout of a label with a glyph missing from gz_20 walks
// the dangling fallback.  Usage: repro bug | repro fix
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
LV_FONT_DECLARE(gz_20)
static uint32_t tick=0; static uint32_t tk(void){return tick;}
static uint8_t buf[466*40*2];
static void flush(lv_display_t* d,const lv_area_t* a,uint8_t* p){(void)a;(void)p;lv_display_flush_ready(d);}
int main(int argc,char** argv){
  int fix = argc>1 && !strcmp(argv[1],"fix");
  lv_init(); lv_tick_set_cb(tk);
  lv_display_t* d=lv_display_create(466,466); lv_display_set_flush_cb(d,flush);
  lv_display_set_buffers(d,buf,NULL,sizeof buf,LV_DISPLAY_RENDER_MODE_PARTIAL);
  // a code point montserrat_14 has but gz_20 lacks -> forces the fallback path
  lv_font_glyph_dsc_t g; uint32_t cp=0;
  static const uint32_t C[]={0xB0,0x7E,0x40,0x23,0x25,0x26,0x5E,0x7C};
  lv_font_t probe=gz_20; probe.fallback=NULL;
  for(unsigned i=0;i<sizeof C/sizeof*C;i++) if(!lv_font_get_glyph_dsc(&probe,&g,C[i],0) && lv_font_get_glyph_dsc(&lv_font_montserrat_14,&g,C[i],0)){cp=C[i];break;}
  if(!cp){ printf("no probe code point found\n"); return 2; }
  static lv_font_t f20; f20=gz_20;
  lv_font_t* theme=malloc(sizeof *theme); *theme=lv_font_montserrat_14;   // LvglBuiltInFont copy
  f20.fallback = fix ? &lv_font_montserrat_14 : theme;                     // fix: never-freed font (or re-pointed by hook)
  lv_obj_t* l=lv_label_create(lv_screen_active()); lv_obj_set_style_text_font(l,&f20,0);
  char t[16];
  // utf8 encode cp
  char u[5]={0}; if(cp<0x80){u[0]=cp;} else {u[0]=0xC0|(cp>>6); u[1]=0x80|(cp&63);} snprintf(t,sizeof t,"12%s",u);
  lv_label_set_text(l,t); lv_refr_now(d);
  printf("U+%04X rendered via fallback before free: ok\n",(unsigned)cp);
  if(!getenv("NOPOISON")) memset(theme,0xff,sizeof *theme); free(theme);                           // SetTextFont(): previous font owner reset
  lv_label_set_text(l,"8"); lv_label_set_text(l,t);                         // any later relayout
  for(int i=0;i<200;i++){ tick+=10; lv_timer_handler(); }
  lv_refr_now(d);
  printf("%s: survived relayout after theme font freed\n",fix?"FIX":"BUG");
  return 0;
}
