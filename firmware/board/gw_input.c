#include "gw_input.h"
gw_tap_verdict_t gw_chat_tap_decide(const gw_tap_t* t){
  if(t->since_wake_us < GW_CHAT_WAKE_GAP_US) return GW_TAP_AFTER_WAKE;
  if(t->since_gesture_us < GW_CHAT_GESTURE_GAP_US) return GW_TAP_AFTER_SWIPE;
  if(t->since_page_us < GW_CHAT_PAGE_GAP_US) return GW_TAP_PAGE_SETTLE;
  int dx=t->x-233, dy=t->y-233; if(dx*dx+dy*dy > GW_CHAT_MAX_R*GW_CHAT_MAX_R) return GW_TAP_EDGE;
  if(!t->long_press || t->press_us < GW_CHAT_MIN_PRESS_US) return GW_TAP_SHORT;
  return GW_TAP_OK;
}
const char* gw_tap_verdict_str(gw_tap_verdict_t v){
  static const char* s[]={"ok","short","edge","after_swipe","after_wake","page_settle"}; return (unsigned)v<6?s[v]:"?"; }
void gw_anim_gov_init(gw_anim_gov_t* g){ g->slide_on=true; g->bad_streak=0; g->last_fps=0; g->samples=0; }
void gw_anim_gov_slide_done(gw_anim_gov_t* g, int frames, int64_t dur_us){
  if(frames<2 || dur_us<=0) return;               // nothing measurable (e.g. page hidden): ignore
  g->samples++; g->last_fps=(int)(frames*1000000LL/dur_us);
  if(g->last_fps<30){ if(++g->bad_streak>=2) g->slide_on=false; } else g->bad_streak=0;
}
