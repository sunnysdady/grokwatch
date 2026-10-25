#include "gw_model.h"
#include <stdio.h>
#include <string.h>
static uint8_t b[400]; int F=0;
#define CK(c) do{ if(!(c)){ printf("FAIL %s\n",#c); F++; } }while(0)
int main(){ int n; int64_t t=1000000;
 const char a[]="350 米\0路\0" "12分钟\0" "21:31\0L=23;C=G"; n=gw_frame(b,0x11,a,sizeof a); gw_model_rx(b,n,t);
 CK(gw_m.light_s==23); CK(gw_m.light_c=='G'); CK(gw_model_light_left(t+5000000)==18); CK(gw_model_light_left(t+30000000)==0);
 const char o[]="350 米\0路\0" "12分钟\0" "21:31"; n=gw_frame(b,0x11,o,sizeof o); gw_model_rx(b,n,t); CK(gw_m.light_s==0); CK(!strcmp(gw_m.eta,"21:31"));
 const char bad[]="1\0r\0d\0e\0L=999"; n=gw_frame(b,0x11,bad,sizeof bad); gw_model_rx(b,n,t); CK(gw_m.light_s==0);
 CK(gw_m.usage_pct==-1); uint8_t u[]={42,'1','8',':','0','0',0}; n=gw_frame(b,0x50,u,sizeof u); CK(gw_model_rx(b,n,t)&GW_EV_USAGE); CK(gw_m.usage_pct==42); CK(!strcmp(gw_m.usage_reset,"18:00"));
 uint8_t u2[]={255}; n=gw_frame(b,0x50,u2,1); gw_model_rx(b,n,t); CK(gw_m.usage_pct==-1); CK(gw_m.usage_reset[0]==0);
 uint8_t z[]={0}; n=gw_frame(b,0x10,z,1); gw_model_rx(b,n,t); CK(gw_model_light_left(t)==0);
 printf("%d failed\n",F); return F; }
