// gw_orient: hysteresis + debounce for the auto 180° flip.
#include <assert.h>
#include <stdio.h>
#include "../gw_orient.h"
static int feed(gw_orient_t* o, float up, int n){ int s=0; while(n--) s=gw_orient_feed(o,up,1.0f); return s; }
int main(void){
  gw_orient_t o={0};
  assert(feed(&o,-0.9f,GW_ORIENT_N-1)==0);          // not yet: debounce
  assert(feed(&o,-0.9f,1)==1);                      // hanging by the lanyard -> flipped
  assert(feed(&o,0.3f,50)==1);                      // tilted but inside the dead band -> keep
  assert(feed(&o,0.0f,50)==1);                      // flat on the table -> keep
  feed(&o,0.9f,GW_ORIENT_N-1); assert(feed(&o,-0.9f,1)==1);   // one bounce back resets the count
  assert(feed(&o,0.9f,GW_ORIENT_N-1)==1);
  assert(gw_orient_feed(&o,0.9f,2.5f)==1);          // a shake sample resets the count, never flips
  assert(feed(&o,0.9f,GW_ORIENT_N-1)==1);
  assert(feed(&o,0.9f,1)==0);                       // upright again
  for(int i=0;i<100;i++) assert(gw_orient_feed(&o,0.9f,0.1f)==0);   // free fall: no opinion
  puts("gw_orient OK"); return 0;
}
