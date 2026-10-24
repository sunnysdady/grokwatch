#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "gw_model.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
gw_model_t gw_m = { .last_m=-1, .phone_batt=-1 };

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
static void hhmm(char* out){ time_t t=time(NULL); struct tm tm; localtime_r(&t,&tm); snprintf(out,6,"%02d:%02d",tm.tm_hour,tm.tm_min); }

static int one(uint8_t t,const uint8_t* p,int n,int64_t now){
  int ev=0, i=0, l;
  switch(t){
  case 0x01: if(n>=1) gw_m.phone_batt=p[0]; break;
  case 0x10: { bool a=n>0&&p[0]; gw_m.last_nav=now; if(a==gw_m.nav_active) break; gw_m.nav_active=a;
      if(a) ev|=GW_EV_NAV_START; else { ev|=GW_EV_NAV_END|summary(now); gw_m.last_m=-1; gw_m.has_icon=false; gw_m.dist[0]=gw_m.road[0]=gw_m.dur[0]=gw_m.eta[0]=0; } break; }
  case 0x11: { gw_m.last_nav=now; const char* s;
      s=cs(p,n,&i,&l); gw_utf8_copy(gw_m.dist,sizeof gw_m.dist,s,l); s=cs(p,n,&i,&l); gw_utf8_copy(gw_m.road,sizeof gw_m.road,s,l);
      s=cs(p,n,&i,&l); gw_utf8_copy(gw_m.dur,sizeof gw_m.dur,s,l);  s=cs(p,n,&i,&l); gw_utf8_copy(gw_m.eta,sizeof gw_m.eta,s,l);
      int m=meters(gw_m.dist); if(m>=0){ if(gw_m.last_m>200&&m<=200) ev|=GW_EV_TURN_SOON; else if(gw_m.last_m>40&&m<=40) ev|=GW_EV_TURN_NOW; gw_m.last_m=m; }
      ev|=GW_EV_NAV_DATA; break; }
  case 0x12: if(n>=97&&p[0]<3){ memcpy(gw_m.icon_buf+96*p[0],p+1,96); if(p[0]==2){ memcpy(gw_m.icon,gw_m.icon_buf,288); gw_m.has_icon=true; ev|=GW_EV_ICON; } } break;
  case 0x20: if(n>=2){ int v=p[0]|p[1]<<8; if(gw_m.spd_t&&v>30) gw_m.ride_km+=v/10.0f*((now-gw_m.spd_t)/3.6e9f); gw_m.spd_t=now; gw_m.last_spd=now;
      if(gw_m.ride_start&&v/10.0f>gw_m.ride_max) gw_m.ride_max=v/10.0f;
      if(v>30){ gw_m.last_move=now; if(!gw_m.ride_start) gw_m.ride_start=now; }
      else if(gw_m.ride_start&&now-gw_m.last_move>600000000LL) ev|=summary(now);
      snprintf(gw_m.speed,sizeof gw_m.speed,"%d",(v+5)/10); ev|=GW_EV_SPEED; } break;
  case 0x30: { if(!gw_m.notes){ gw_m.notes=(gw_note_t*)calloc(GW_MAX_NOTES,sizeof(gw_note_t)); if(!gw_m.notes) break; }
      gw_note_t nt; const char* s;
      s=cs(p,n,&i,&l); gw_utf8_copy(nt.app,sizeof nt.app,s,l); s=cs(p,n,&i,&l); gw_utf8_copy(nt.title,sizeof nt.title,s,l);
      s=cs(p,n,&i,&l); gw_utf8_copy(nt.body,sizeof nt.body,s,l); hhmm(nt.time);
      // same app+title replaces the older entry (WeChat updates one notification per chat)
      int k; for(k=0;k<gw_m.n_notes;k++) if(!strcmp(gw_m.notes[k].app,nt.app)&&!strcmp(gw_m.notes[k].title,nt.title)) break;
      if(k==gw_m.n_notes){ if(gw_m.n_notes<GW_MAX_NOTES) gw_m.n_notes++; k=gw_m.n_notes-1; }
      memmove(&gw_m.notes[1],&gw_m.notes[0],sizeof(gw_note_t)*k); gw_m.notes[0]=nt; ev|=GW_EV_NOTE; break; }
  case 0x40: if(n>=4){ gw_m.time_unix=p[0]|p[1]<<8|p[2]<<16|(uint32_t)p[3]<<24; gw_m.tz_min=n>=6?(int16_t)(p[4]|p[5]<<8):480; ev|=GW_EV_TIME; } break;
  }
  return ev;
}
int gw_model_rx(const uint8_t* d,int n,int64_t now){ gw_m.last_rx=now; int i=0, ev=0;
  while(i+3<=n){ int l=d[i+1]|d[i+2]<<8; if(i+3+l>n) break; ev|=one(d[i],d+i+3,l,now); i+=3+l; } return ev; }
int gw_model_tick(int64_t now){ int ev=0;
  if(gw_m.last_spd&&now-gw_m.last_spd>5000000){ gw_m.speed[0]=0; gw_m.last_spd=0; ev|=GW_EV_SPEED; }
  if(gw_m.nav_active&&now-gw_m.last_nav>20000000){ gw_m.nav_active=false; gw_m.has_icon=false; gw_m.last_m=-1; ev|=GW_EV_NAV_END; }
  return ev; }
bool gw_model_linked(bool ble,int64_t now){ return ble&&gw_m.last_rx&&now-gw_m.last_rx<10000000; }
int gw_model_ride_min(int64_t now){ return gw_m.ride_start?(int)((now-gw_m.ride_start)/60000000):0; }
void gw_model_clear_notes(void){ gw_m.n_notes=0; }
