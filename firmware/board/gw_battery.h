#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Voltage-only estimate, not a fuel gauge. Filter once per second regardless
 * of reader count; never convert charging voltage steps into instant capacity. */
typedef struct { bool ready; int mv_q8,level; int64_t sample_ms,change_ms; } gw_battery_t;
static inline int gw_battery_update(gw_battery_t* b,int mv,bool external,int64_t now){
    if(mv<2500||mv>4500) return b->ready?b->level:-1;
    int raw=mv<=3400?0:mv>=4200?100:(mv-3400)/8;
    if(!b->ready){b->ready=true;b->mv_q8=mv*256;b->level=raw;b->sample_ms=b->change_ms=now;return raw;}
    if(now-b->sample_ms<1000) return b->level;
    b->sample_ms=now;b->mv_q8+=(mv*256-b->mv_q8)/8;
    int filtered=b->mv_q8/256;
    int target=filtered<=3400?0:filtered>=4200?100:(filtered-3400)/8;
    // One percentage point per 30s at most. No catch-up jumps after sleep.
    if(now-b->change_ms>=30000){
        if(target<b->level){--b->level;b->change_ms=now;}
        else if(external && target>b->level){++b->level;b->change_ms=now;}
    }
    return b->level;
}
