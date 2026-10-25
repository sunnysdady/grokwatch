// 2.6.24 snapshot page slide: the outgoing and incoming screens are rendered once into two RGB565 bitmaps in PSRAM,
// then only those two bitmaps move. Each slide frame is two opaque image copies instead of a full widget re-render.
#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
#define GW_SLIDE_W 466
#define GW_SLIDE_DUR 380          // ms, ease-in-out
#define GW_SLIDE_PARALLAX 140     // outgoing page drifts this far (px) while the new one covers it
void gw_slide_prepare(void);      // touch-down: snapshot the current page now so the swipe does not wait for it
bool gw_slide_snap_old(void);     // call BEFORE the page changes; false = no buffers, switch directly
bool gw_slide_start(int dir);     // call AFTER the new page is rendered into the widget tree; +1 = new page from the right
bool gw_slide_active(void);
// Direct-push mode (2.6.25): when the board provides a panel path, the slide bypasses LVGL entirely. Every frame is
// composed row by row from the two PSRAM snapshots into small DMA strips and pushed straight to the panel; LVGL does
// no drawing during the slide. gw_slide_start() then runs the whole animation synchronously and returns when done.
typedef struct {
  int rows;                                   // rows per strip (even); two strip buffers
  uint8_t* (*buf)(int i);                     // i = 0/1: DMA-capable strip buffer of rows*GW_SLIDE_W*2 bytes
  void (*push)(int y0,int n,uint8_t* b);      // start sending rows [y0,y0+n) full width, returns without waiting
  void (*wait)(void);                         // block until the previous push finished
  int64_t (*now)(void);                       // optional clock (us); NULL = real clock
  void (*frame_done)(void);                   // optional, after each composed frame
} gw_slide_hw_t;
void gw_slide_set_hw(const gw_slide_hw_t* hw);
void gw_slide_last_stats(int* frames,int* ms,int* max_frame_us);   // direct mode: last slide
void gw_slide_last_split(int* compose_ms,int* wait_ms);   // direct mode: time composing vs waiting for the bus
void gw_slide_set_style(int s);   // 0 legacy disc cards, 1 cover (default)
void gw_slide_cancel(void);       // jump to the end (real page shows)
void gw_slide_set_done_cb(void (*cb)(void));
int  gw_slide_last_snap_us(int which);   // 0 old, 1 new: time to render the snapshot
#ifdef __cplusplus
}
#endif
