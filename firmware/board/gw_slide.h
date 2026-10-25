// 2.6.24 snapshot page slide: the outgoing and incoming screens are rendered once into two RGB565 bitmaps in PSRAM,
// then only those two bitmaps move. Each slide frame is two opaque image copies instead of a full widget re-render.
#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
#define GW_SLIDE_W 466
#define GW_SLIDE_DUR 300          // ms, ease-out
#define GW_SLIDE_PARALLAX 140     // outgoing page drifts this far (px) while the new one covers it
bool gw_slide_snap_old(void);     // call BEFORE the page changes; false = no buffers, switch directly
bool gw_slide_start(int dir);     // call AFTER the new page is rendered into the widget tree; +1 = new page from the right
bool gw_slide_active(void);
void gw_slide_cancel(void);       // jump to the end (real page shows)
void gw_slide_set_done_cb(void (*cb)(void));
int  gw_slide_last_snap_us(int which);   // 0 old, 1 new: time to render the snapshot
#ifdef __cplusplus
}
#endif
