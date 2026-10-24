// Host unit tests for gw_model (pure C). Build: make -C main/boards/m5stack/stopwatch/test
#include "../gw_model.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int fails=0, checks=0;
#define CHECK(c,msg) do{ checks++; if(!(c)){ fails++; printf("  FAIL %s:%d %s\n",__FILE__,__LINE__,msg);} }while(0)
#define S(x) ((int64_t)(x)*1000000LL)
static void reset(void){ memset(&gw_m,0,sizeof gw_m); gw_m.last_m=-1; gw_m.phone_batt=-1; gw_model_reset(); }
static int tx(uint8_t t,const void* p,int l,int64_t now){ uint8_t b[700]; int n=gw_frame(b,t,p,l); return gw_model_rx(b,n,now); }
static int nav_on(int64_t now){ uint8_t a=1; return tx(0x10,&a,1,now); }
static int nav_off(int64_t now){ uint8_t a=0; return tx(0x10,&a,1,now); }
static int nav(const char* d,const char* road,const char* dur,const char* eta,int64_t now){ char b[256]; int n=0;
  #define P(s) { int l=strlen(s); memcpy(b+n,s,l); n+=l; b[n++]=0; }
  P(d) P(road) P(dur) P(eta) return tx(0x11,b,n,now); }
static int spd(int kmh10,int64_t now){ uint8_t b[2]={kmh10&0xFF,kmh10>>8}; return tx(0x20,b,2,now); }
#define ALERTS(ev) (((ev)&GW_EV_TURN_SOON)?1:0)+(((ev)&GW_EV_TURN_NOW)?1:0)

static void t_first_entry(void){ puts("first entry inside a zone");
  reset(); nav_on(S(1)); int ev=nav("30 m","A路","3分钟","21:30",S(1)); CHECK(ev&GW_EV_TURN_NOW,"first packet at 30 m must alert NOW");
  reset(); nav_on(S(1)); ev=nav("150 m","A路","3分钟","21:30",S(1)); CHECK(ev&GW_EV_TURN_SOON,"first packet at 150 m must alert SOON");
  reset(); nav_on(S(1)); ev=nav("1.2 公里","A路","3分钟","21:30",S(1)); CHECK(ALERTS(ev)==0,"first packet far away: no alert"); }
static void t_jump(void){ puts("threshold jump 350 -> 30");
  reset(); nav_on(S(1)); nav("350 m","A路","","",S(1)); int ev=nav("30 m","A路","","",S(2));
  CHECK(ev&GW_EV_TURN_NOW,"350->30 must alert NOW"); CHECK(!(ev&GW_EV_TURN_SOON),"jump fires only the most urgent alert");
  ev=nav("20 m","A路","","",S(3)); CHECK(ALERTS(ev)==0,"no repeat after NOW"); }
static void t_jitter(void){ puts("jitter 210/190 and 45/35");
  reset(); nav_on(S(1)); int n=0; const int seq[]={260,210,190,210,190,210,195,205,190,180};
  for(int i=0;i<10;i++){ char d[16]; sprintf(d,"%d m",seq[i]); n+=(nav(d,"A路","","",S(2+i))&GW_EV_TURN_SOON)?1:0; }
  CHECK(n==1,"SOON fires exactly once across 210/190 jitter");
  n=0; const int s2[]={60,45,35,45,35,42,38}; for(int i=0;i<7;i++){ char d[16]; sprintf(d,"%d m",s2[i]); n+=(nav(d,"A路","","",S(20+i))&GW_EV_TURN_NOW)?1:0; }
  CHECK(n==1,"NOW fires exactly once across 45/35 jitter"); }
static void t_next_maneuver(void){ puts("next maneuver / reroute");
  reset(); nav_on(S(1)); nav("150 m","A路","","",S(1)); nav("20 m","A路","","",S(2));
  int ev=nav("800 m","B路","","",S(3)); CHECK(ALERTS(ev)==0,"new far maneuver: no alert");
  ev=nav("190 m","B路","","",S(10)); CHECK(ev&GW_EV_TURN_SOON,"next maneuver alerts SOON again");
  ev=nav("600 m","B路","","",S(11)); CHECK(ALERTS(ev)==0,"reroute (distance jumps up): no alert");
  ev=nav("180 m","B路","","",S(15)); CHECK(ev&GW_EV_TURN_SOON,"after reroute the re-approach alerts again"); }
static void t_reconnect(void){ puts("link drop and reconnect");
  reset(); nav_on(S(1)); nav("150 m","A路","","",S(1));
  int ev=gw_model_tick(S(30)); CHECK(ev&GW_EV_NAV_END,"20 s without data ends nav"); CHECK(!gw_m.nav_active,"inactive after timeout");
  nav_on(S(31)); ev=nav("140 m","A路","","",S(31)); CHECK(gw_m.nav_active,"replay restores nav"); CHECK(ALERTS(ev)==0,"same maneuver after reconnect: no duplicate SOON");
  ev=nav("30 m","A路","","",S(40)); CHECK(ev&GW_EV_TURN_NOW,"still alerts NOW later"); }
static void t_stale_replay(void){ puts("phone keeps replaying identical old nav");
  reset(); nav_on(S(0)); nav("500 m","A路","10分钟","21:30",S(0)); int end=0;
  for(int t=5;t<=600;t+=5){ nav_on(S(t)); nav("500 m","A路","10分钟","21:30",S(t)); end|=gw_model_tick(S(t)); }
  CHECK(!gw_m.nav_active,"identical content for 10 min is expired"); CHECK(end&GW_EV_NAV_END,"expiry raises NAV_END");
  nav_on(S(601)); nav("480 m","A路","10分钟","21:30",S(601)); CHECK(gw_m.nav_active,"new content revives nav");
  reset(); nav_on(S(0)); for(int t=0;t<=600;t+=5){ char eta[8]; sprintf(eta,"21:%02d",30+t/60); nav_on(S(t)); nav("500 m","A路","10分钟",eta,S(t)); gw_model_tick(S(t)); }
  CHECK(gw_m.nav_active,"waiting at a light with changing ETA stays active"); }
static void t_speed_gap(void){ puts("speed stream gap");
  reset(); for(int t=0;t<=60;t++) spd(360,S(t)); float km1=gw_m.ride_km; CHECK(km1>0.59f&&km1<0.61f,"60 s at 36 km/h = 0.6 km");
  spd(360,S(120)); CHECK(gw_m.ride_km-km1<0.005f,"60 s gap is not integrated");
  for(int t=121;t<=130;t++){ spd(360,S(t)); } CHECK(gw_m.ride_km-km1>0.09f&&gw_m.ride_km-km1<0.11f,"resumes integrating after gap"); }
static void t_unknown_units(void){ puts("unparseable distance");
  reset(); nav_on(S(1)); int ev=nav("现在","A路","","",S(1)); CHECK(ALERTS(ev)==0,"no alert for unparseable distance"); CHECK(ev&GW_EV_NAV_DATA,"still data event"); }
static void t_property_noise(void){ puts("property: noisy approach, 500 random rides");
  srand(42); int bad=0;
  for(int r=0;r<500;r++){ reset(); nav_on(S(1)); int soon=0,now=0; int t=2;
    for(int d=600; d>=0; d-=5+rand()%20){ int m=d+(rand()%31)-15; if(m<0)m=0; char s[16]; sprintf(s,"%d m",m);
      int ev=nav(s,"A路","","",S(t++)); soon+=!!(ev&GW_EV_TURN_SOON); now+=!!(ev&GW_EV_TURN_NOW); }
    if(now!=1||soon>1) bad++; }
  CHECK(bad==0,"each noisy approach: exactly one NOW, at most one SOON"); }
static void t_explicit_end(void){ puts("explicit end then same road again");
  reset(); nav_on(S(1)); nav("30 m","A路","","",S(1)); nav_off(S(2)); CHECK(!gw_m.nav_active,"ended");
  nav_on(S(3)); int ev=nav("30 m","A路","","",S(3)); CHECK(ev&GW_EV_TURN_NOW,"new trip re-alerts"); }
static void t_legacy_notify(void){ puts("legacy NOTIFY frame ignored, nav in same packet still parsed");
  reset(); uint8_t b[600]; int n=0; uint8_t a=1; n+=gw_frame(b+n,0x10,&a,1);
  const char nt[]="微信\0张三\0吃饭？\0"; n+=gw_frame(b+n,0x30,nt,sizeof nt);
  const char nv[]="30 m\0A路\0\0\0"; n+=gw_frame(b+n,0x11,nv,sizeof nv);
  int ev=gw_model_rx(b,n,S(1)); CHECK(gw_m.nav_active,"nav active"); CHECK(ev&GW_EV_TURN_NOW,"turn alert still fires"); CHECK(!strcmp(gw_m.road,"A路"),"road parsed after notify frame"); }
int main(void){ t_legacy_notify(); t_property_noise(); t_explicit_end(); t_first_entry(); t_jump(); t_jitter(); t_next_maneuver(); t_reconnect(); t_stale_replay(); t_speed_gap(); t_unknown_units();
  printf("%d checks, %d failed\n",checks,fails); return fails?1:0; }
