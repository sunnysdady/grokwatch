#include "gw_health.h"
static volatile uint32_t beats;
void gw_health_ui_beat(void){ beats++; }
uint32_t gw_health_ui_beats(void){ return beats; }
gw_health_verdict_t gw_health_decide(const gw_health_sample_t* s){
  bool ok = s->ui_beats >= GW_H_MIN_UI_BEATS && s->heap_min_free >= GW_H_MIN_HEAP && s->heap_free >= GW_H_MIN_HEAP;
  if(s->uptime_s >= GW_H_MIN_UPTIME_S && ok) return GW_H_VALID;
  if(s->uptime_s >= GW_H_DEADLINE_S) return GW_H_ROLLBACK;
  return GW_H_WAIT;
}

#include <stdio.h>
bool gw_bootrec_is_crash_rr(uint32_t rr){ return rr==4||rr==5||rr==6||rr==7||rr==9; }
void gw_bootrec_on_boot(gw_bootrec_t* r, uint32_t rr, bool pending){
  if(r->cur_pending && !r->cur_validated){ r->pv_failed++; r->fail_up_s=r->cur_up_s; r->fail_heap_min=r->cur_heap_min; r->fail_beats=r->cur_beats; }
  r->boots++; r->last_rr=rr; if(gw_bootrec_is_crash_rr(rr)) r->crash_rr_count++;
  if(pending) r->pv_boots++;
  r->cur_pending=pending?1:0; r->cur_validated=0; r->cur_up_s=0; r->cur_heap_min=0; r->cur_beats=0; }
void gw_bootrec_checkpoint(gw_bootrec_t* r, uint32_t up, uint32_t hm, uint32_t b){ r->cur_up_s=up; r->cur_heap_min=hm; r->cur_beats=b; }
void gw_bootrec_validated(gw_bootrec_t* r){ r->cur_validated=1; }
int gw_bootrec_json(const gw_bootrec_t* r, char* o, int cap){
  return snprintf(o,cap,"{\"boots\":%lu,\"rr\":%lu,\"crash_boots\":%lu,\"pv_boots\":%lu,\"pv_failed\":%lu,\"fail_up_s\":%lu,\"fail_heap_min\":%lu,\"fail_ui\":%lu}",
    (unsigned long)r->boots,(unsigned long)r->last_rr,(unsigned long)r->crash_rr_count,(unsigned long)r->pv_boots,(unsigned long)r->pv_failed,
    (unsigned long)r->fail_up_s,(unsigned long)r->fail_heap_min,(unsigned long)r->fail_beats); }
