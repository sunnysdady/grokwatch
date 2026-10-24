#include "../gw_input.h"
#include <stdio.h>
static int f=0,n=0;
#define CHECK(c,m) do{ n++; if(!(c)){ f++; printf("  FAIL %d %s\n",__LINE__,m);} }while(0)
static gw_tap_t T(int64_t press,int x,int y,int64_t g,int64_t w,int64_t p,bool lp){ gw_tap_t t={0,press,x,y,g,w,p,lp}; return t; }
#define S 1000000
int main(void){ puts("chat trigger rule + anim governor");
  CHECK(gw_chat_tap_decide(&(gw_tap_t){0,600000,233,233,5*S,5*S,5*S,true})==GW_TAP_OK,"deliberate centre long-press triggers");
  gw_tap_t a=T(80000,233,233,5*S,5*S,5*S,false); CHECK(gw_chat_tap_decide(&a)==GW_TAP_SHORT,"plain tap no longer triggers Grok");
  a=T(700000,233,233,5*S,300000,5*S,true); CHECK(gw_chat_tap_decide(&a)==GW_TAP_AFTER_WAKE,"the tap that wakes the screen is ignored");
  a=T(700000,233,233,200000,5*S,5*S,true); CHECK(gw_chat_tap_decide(&a)==GW_TAP_AFTER_SWIPE,"press right after a swipe ignored");
  a=T(700000,233,233,5*S,5*S,100000,true); CHECK(gw_chat_tap_decide(&a)==GW_TAP_PAGE_SETTLE,"press while the page is still landing ignored");
  a=T(700000,20,233,5*S,5*S,5*S,true); CHECK(gw_chat_tap_decide(&a)==GW_TAP_EDGE,"bezel-edge graze ignored");
  a=T(700000,233+169,233,5*S,5*S,5*S,true); CHECK(gw_chat_tap_decide(&a)==GW_TAP_OK,"just inside radius ok");
  a=T(700000,233+171,233,5*S,5*S,5*S,true); CHECK(gw_chat_tap_decide(&a)==GW_TAP_EDGE,"just outside radius rejected");
  a=T(499999,233,233,5*S,5*S,5*S,true); CHECK(gw_chat_tap_decide(&a)==GW_TAP_SHORT,"below hold threshold");
  gw_anim_gov_t g; gw_anim_gov_init(&g); CHECK(g.slide_on,"slides on by default");
  gw_anim_gov_slide_done(&g,10,240000); CHECK(g.slide_on&&g.last_fps==41,"41 fps slide keeps animation");
  gw_anim_gov_slide_done(&g,6,240000); CHECK(g.slide_on&&g.bad_streak==1,"one slow slide is tolerated");
  gw_anim_gov_slide_done(&g,10,240000); CHECK(g.bad_streak==0,"good slide resets streak");
  gw_anim_gov_slide_done(&g,6,240000); gw_anim_gov_slide_done(&g,7,240000); CHECK(!g.slide_on,"2 slides <30 fps -> direct switch");
  gw_anim_gov_slide_done(&g,20,240000); CHECK(!g.slide_on,"stays off for this boot (no flapping)");
  gw_anim_gov_init(&g); gw_anim_gov_slide_done(&g,1,240000); gw_anim_gov_slide_done(&g,0,0); CHECK(g.slide_on&&g.samples==0,"unmeasurable slides ignored");
  printf("%d checks, %d failed\n",n,f); return f?1:0; }
