#include "gw_slide.h"
#include "gw_mem.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
static int64_t us(void){ return esp_timer_get_time(); }
#else
#include <time.h>
static int64_t us(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
#endif
#define W GW_SLIDE_W
#define BUFSZ ((size_t)W*W*2)
static lv_draw_buf_t sb_old, sb_new; static uint8_t *m_old, *m_new; static bool have_old;
static lv_obj_t *ov, *im_old, *im_new; static int s_dir=1; static void (*done_cb)(void); static int snap_us[2];
static bool bufs(void){
  if(!m_old) m_old=(uint8_t*)gw_psram_calloc(BUFSZ);
  if(!m_new) m_new=(uint8_t*)gw_psram_calloc(BUFSZ);
  return m_old&&m_new; }
static bool snap(lv_draw_buf_t* db,uint8_t* mem,int* t){ int64_t t0=us();
  lv_draw_buf_init(db,W,W,LV_COLOR_FORMAT_RGB565,W*2,mem,BUFSZ);
  lv_obj_t* scr=lv_screen_active(); lv_obj_update_layout(scr);
  lv_result_t r=lv_snapshot_take_to_draw_buf(scr,LV_COLOR_FORMAT_RGB565,db); *t=(int)(us()-t0); return r==LV_RESULT_OK; }
bool gw_slide_active(void){ return ov!=NULL; }
void gw_slide_set_done_cb(void (*cb)(void)){ done_cb=cb; }
int gw_slide_last_snap_us(int which){ return snap_us[which?1:0]; }
static void finish(void){ if(!ov) return; lv_anim_delete(ov,NULL); lv_obj_delete(ov); ov=im_old=im_new=NULL; if(done_cb) done_cb(); }
void gw_slide_cancel(void){ finish(); }
static void ax(void* var,int32_t v){ (void)var; if(!ov) return;   // v: 0..1000 progress
  int x_new=s_dir*(W-(W*v)/1000), x_old=-s_dir*(GW_SLIDE_PARALLAX*v)/1000;
  lv_obj_set_x(im_new,x_new); lv_obj_set_x(im_old,x_old); }
static void anim_done(lv_anim_t* a){ (void)a; finish(); }
bool gw_slide_snap_old(void){ if(ov) finish(); have_old=false; if(!bufs()) return false;
  have_old=snap(&sb_old,m_old,&snap_us[0]); return have_old; }
bool gw_slide_start(int dir){ if(!have_old) return false; have_old=false; s_dir=dir<0?-1:1;
  if(!snap(&sb_new,m_new,&snap_us[1])) return false;
  ov=lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(ov); lv_obj_set_size(ov,W,W); lv_obj_set_pos(ov,0,0);
  lv_obj_set_style_bg_color(ov,lv_color_black(),0); lv_obj_set_style_bg_opa(ov,LV_OPA_COVER,0);   // opaque: nothing under it is redrawn
  lv_obj_remove_flag(ov,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  // raw RGB565 in RAM: the built-in decoder reads it in place (no decoded copy to invalidate)
  im_old=lv_image_create(ov); lv_image_set_src(im_old,&sb_old); lv_obj_set_pos(im_old,0,0);
  im_new=lv_image_create(ov); lv_image_set_src(im_new,&sb_new); lv_obj_set_pos(im_new,s_dir*W,0);
  lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,ov); lv_anim_set_exec_cb(&a,ax); lv_anim_set_values(&a,0,1000);
  lv_anim_set_duration(&a,GW_SLIDE_DUR); lv_anim_set_path_cb(&a,lv_anim_path_ease_out); lv_anim_set_completed_cb(&a,anim_done); lv_anim_start(&a);
  return true; }
