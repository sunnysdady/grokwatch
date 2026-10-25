#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "gw_model.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
gw_model_t gw_m = { .last_m=-1, .phone_batt=-1, .usage_pct=-1 };

void gw_utf8_copy(char* dst, int cap, const char* src, int n){
  if(cap<=0) return; if(n>cap-1) { n=cap-1; while(n>0 && ((unsigned char)src[n]&0xC0)==0x80) n--; }
  memcpy(dst,src,n); dst[n]=0;
}
int gw_frame(uint8_t* o, uint8_t t, const void* p, int l){ o[0]=t; o[1]=l&0xFF; o[2]=l>>8; memcpy(o+3,p,l); return l+3; }
static const char* cs(const uint8_t* p,int n,int* i,int* len){ int s=*i; while(*i<n&&p[*i]) (*i)++; *len=*i-s; if(*i<n) (*i)++; return (const char*)p+s; }
static int meters(const char* d){ float v=0; if(sscanf(d,"%f",&v)<1) return -1; const char* u=d; while(*u&&(strchr("0123456789. ",*u))) u++;
  if(!strncmp(u,"公里",6)||!strncmp(u,"千米",6)||!strncmp(u,"km",2)||!strncmp(u,"KM",2)) return (int)(v*1000); return (int)v; }
static int summary(int64_t now){
  if(!gw_m.ride_start) return 0; int64_t dur=(now-gw_m.ride_start)/1000000; int ev=0;
  if(dur>=120){ float avg=gw_m.ride_km/(dur/3600.0f);
    snprintf(gw_m.summary,sizeof gw_m.summary,"用时 %d:%02d  ·  %.1f km\n最高 %.0f  ·  平均 %.0f km/h",(int)(dur/3600),(int)(dur/60%60),gw_m.ride_km,gw_m.ride_max,avg); ev=GW_EV_SUMMARY; }
  gw_m.ride_start=0; gw_m.ride_max=0; gw_m.ride_km=0; return ev; }

static uint32_t fnv(const uint8_t* p,int n){ uint32_t h=2166136261u; for(int i=0;i<n;i++){ h^=p[i]; h*=16777619u; } return h; }
static void rearm(void){ gw_m.alert_lvl=0; gw_m.man_min_m=-1; gw_m.man_road[0]=0; }
static int zone(int m){ return m<0?0: m<=GW_NOW_M?2: m<=GW_SOON_M?1:0; }
// Turn alerts: one state machine per maneuver. Fires the most urgent zone entered that has not fired yet
// (first packet already inside a zone, or a 350->30 jump, still alerts). Jitter around a threshold cannot
// re-fire because the level only goes up until the maneuver changes. A maneuver changes when the road
// changes or the distance grows by >= GW_REARM_M over the closest distance seen (next turn / re-route).
static int turn_alert(int m){
  if(m<0) return 0;
  bool newman = gw_m.man_min_m<0 || strcmp(gw_m.man_road,gw_m.road) || m>=gw_m.man_min_m+GW_REARM_M;
  if(newman){ rearm(); gw_m.man_min_m=m; snprintf(gw_m.man_road,sizeof gw_m.man_road,"%s",gw_m.road); }
  if(m<gw_m.man_min_m) gw_m.man_min_m=m;
  gw_m.last_m=m; int z=zone(m); if(z<=gw_m.alert_lvl) return 0; gw_m.alert_lvl=z;
  return z==2?GW_EV_TURN_NOW:GW_EV_TURN_SOON; }
void gw_model_reset(void){ gw_m.nav_active=false; gw_m.last_m=-1; rearm(); gw_m.nav_sig=0; gw_m.nav_sig_t=0; gw_m.spd_have=false; }
static int one(uint8_t t,const uint8_t* p,int n,int64_t now){
  int ev=0, i=0, l;
  switch(t){
  case 0x01: if(n>=1) gw_m.phone_batt=p[0]; break;
  case 0x10: { bool a=n>0&&p[0]; gw_m.last_nav=now;
      if(a){ bool stale=gw_m.nav_sig_t&&now-gw_m.nav_sig_t>=(int64_t)GW_NAV_STALE_S*1000000; if(gw_m.nav_active||stale) break; gw_m.nav_active=true; ev|=GW_EV_NAV_START; break; }
      if(!gw_m.nav_active&&!gw_m.nav_sig_t) break;
      bool was=gw_m.nav_active; gw_m.nav_active=false; if(was) ev|=GW_EV_NAV_END|summary(now);
      gw_m.last_m=-1; rearm(); gw_m.nav_sig=0; gw_m.nav_sig_t=0; gw_m.has_icon=false; gw_m.light_s=0; gw_m.dist[0]=gw_m.road[0]=gw_m.dur[0]=gw_m.eta[0]=0; break; }
  case 0x11: { gw_m.last_nav=now; const char* s;
      s=cs(p,n,&i,&l); gw_utf8_copy(gw_m.dist,sizeof gw_m.dist,s,l); s=cs(p,n,&i,&l); gw_utf8_copy(gw_m.road,sizeof gw_m.road,s,l);
      s=cs(p,n,&i,&l); gw_utf8_copy(gw_m.dur,sizeof gw_m.dur,s,l);  s=cs(p,n,&i,&l); gw_utf8_copy(gw_m.eta,sizeof gw_m.eta,s,l);
      gw_m.light_s=0; gw_m.light_c=0; gw_m.turn[0]=0;                 // optional 5th string "k=v;k=v" (docs/nav-0x11-ext.md); absent = old app
      if(i<n){ s=cs(p,n,&i,&l); for(int k=0;k+1<l;){ int e=k; while(e<l&&s[e]!=';') e++;
          if(s[k]=='L'&&s[k+1]=='='){ int v=atoi(s+k+2); if(v>0&&v<=300){ gw_m.light_s=v; gw_m.light_t=now; } }
          if(s[k]=='T'&&s[k+1]=='='){ int m=e-k-2; if(m>0&&m<4){ memcpy(gw_m.turn,s+k+2,m); gw_m.turn[m]=0; } }
          if(s[k]=='C'&&s[k+1]=='='&&k+2<e&&strchr("RGY",s[k+2])) gw_m.light_c=s[k+2];
          k=e+1; } }
      uint32_t sig=fnv(p,n); if(sig!=gw_m.nav_sig||!gw_m.nav_sig_t){ gw_m.nav_sig=sig; gw_m.nav_sig_t=now; }
      if(!gw_m.nav_active && now-gw_m.nav_sig_t<(int64_t)GW_NAV_STALE_S*1000000){ gw_m.nav_active=true; ev|=GW_EV_NAV_START; }
      ev|=turn_alert(meters(gw_m.dist));
      ev|=GW_EV_NAV_DATA; break; }
  case 0x12: if(n>=97&&p[0]<3){ memcpy(gw_m.icon_buf+96*p[0],p+1,96); if(p[0]==2){ memcpy(gw_m.icon,gw_m.icon_buf,288); gw_m.has_icon=true; ev|=GW_EV_ICON; } } break;
  case 0x20: if(n>=2){ int v=p[0]|p[1]<<8; int64_t dt=now-gw_m.spd_t; if(gw_m.spd_have&&v>30&&dt>0&&dt<=(int64_t)GW_SPD_GAP_S*1000000) gw_m.ride_km+=v/10.0f*(dt/3.6e9f); gw_m.spd_t=now; gw_m.spd_have=true; gw_m.last_spd=now;
      if(gw_m.ride_start&&v/10.0f>gw_m.ride_max) gw_m.ride_max=v/10.0f;
      if(v>30){ gw_m.last_move=now; if(!gw_m.ride_start) gw_m.ride_start=now; }
      else if(gw_m.ride_start&&now-gw_m.last_move>600000000LL) ev|=summary(now);
      snprintf(gw_m.speed,sizeof gw_m.speed,"%d.%d",v/10,v%10);   /* 2.6.23: keep 0.1 km/h (frame is km/h x10) */ ev|=GW_EV_SPEED; } break;
  case 0x50: if(n>=1){ gw_m.usage_pct=p[0]<=100?p[0]:-1; i=1; const char* r=cs(p,n,&i,&l); gw_utf8_copy(gw_m.usage_reset,sizeof gw_m.usage_reset,r,n>1?l:0); ev|=GW_EV_USAGE; } break;   // docs/usage-frame.md
  case 0x30: break;   // legacy NOTIFY frame from old apps: ignored (notifications removed in 2.6.2)
  case 0x40: if(n>=4){ gw_m.time_unix=p[0]|p[1]<<8|p[2]<<16|(uint32_t)p[3]<<24; gw_m.tz_min=n>=6?(int16_t)(p[4]|p[5]<<8):480; ev|=GW_EV_TIME; } break;
  }
  return ev;
}
int gw_model_rx(const uint8_t* d,int n,int64_t now){ gw_m.last_rx=now; int i=0, ev=0;
  while(i+3<=n){ int l=d[i+1]|d[i+2]<<8; if(i+3+l>n) break; ev|=one(d[i],d+i+3,l,now); i+=3+l; } return ev; }
int gw_model_tick(int64_t now){ int ev=0;
  if(gw_m.last_spd&&now-gw_m.last_spd>5000000){ gw_m.speed[0]=0; gw_m.last_spd=0; ev|=GW_EV_SPEED; }
  // link timeout keeps maneuver alert state (a replay of the same turn after reconnect must not re-alert)
  if(gw_m.nav_active&&now-gw_m.last_nav>(int64_t)GW_NAV_TIMEOUT_S*1000000){ gw_m.nav_active=false; ev|=GW_EV_NAV_END; }
  // phone keeps resending identical content (stale notification): expire it
  if(gw_m.nav_active&&gw_m.nav_sig_t&&now-gw_m.nav_sig_t>=(int64_t)GW_NAV_STALE_S*1000000){ gw_m.nav_active=false; gw_m.has_icon=false; ev|=GW_EV_NAV_END; }
  return ev; }
bool gw_model_linked(bool ble,int64_t now){ return ble&&gw_m.last_rx&&now-gw_m.last_rx<10000000; }
int gw_model_light_left(int64_t now){ if(!gw_m.nav_active||gw_m.light_s<=0) return 0; int left=gw_m.light_s-(int)((now-gw_m.light_t)/1000000); return left>0?left:0; }
int gw_model_ride_min(int64_t now){ return gw_m.ride_start?(int)((now-gw_m.ride_start)/60000000):0; }
