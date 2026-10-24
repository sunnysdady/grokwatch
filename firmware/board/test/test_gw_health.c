#include "../gw_health.h"
#include <stdio.h>
#include <string.h>
static int f=0,n=0;
#define CHECK(c,m) do{ n++; if(!(c)){ f++; printf("  FAIL %d %s\n",__LINE__,m);} }while(0)
static gw_health_verdict_t d(uint32_t up,uint32_t beats,uint32_t heap,uint32_t minh){ gw_health_sample_t s={up,beats,heap,minh}; return gw_health_decide(&s); }
int main(void){ puts("boot health (OTA rollback self-check)");
  CHECK(d(5,5,100000,90000)==GW_H_WAIT,"too early");
  CHECK(d(30,28,100000,90000)==GW_H_VALID,"healthy offline boot validates (no server criterion exists)");
  CHECK(d(60,0,100000,90000)==GW_H_WAIT,"UI not ticking yet: wait");
  CHECK(d(120,0,100000,90000)==GW_H_ROLLBACK,"UI dead by deadline: roll back");
  CHECK(d(120,100,100000,10000)==GW_H_ROLLBACK,"heap exhausted at some point: roll back");
  CHECK(d(45,40,10000,10000)==GW_H_WAIT,"low heap before deadline: wait");
  CHECK(d(119,100,100000,90000)==GW_H_VALID,"recovers before deadline");
  for(int i=0;i<25;i++) gw_health_ui_beat(); CHECK(gw_health_ui_beats()==25,"beat counter");
  { gw_bootrec_t r={0}; char j[256];
    gw_bootrec_on_boot(&r,1,false); CHECK(r.boots==1&&r.pv_failed==0,"power-on boot");
    gw_bootrec_on_boot(&r,3,true); gw_bootrec_checkpoint(&r,18,41000,16);            /* fresh OTA, dies at ~18 s */
    gw_bootrec_on_boot(&r,4,true);                                                    /* next new-image boot after a panic */
    CHECK(r.pv_failed==1&&r.fail_up_s==18&&r.fail_heap_min==41000&&r.fail_beats==16,"unvalidated pending boot is counted with its last checkpoint");
    CHECK(r.crash_rr_count==1&&r.last_rr==4,"panic reset reason counted");
    gw_bootrec_checkpoint(&r,30,40000,29); gw_bootrec_validated(&r); gw_bootrec_on_boot(&r,3,false);
    CHECK(r.pv_failed==1,"validated pending boot is not a failure");
    gw_bootrec_on_boot(&r,6,false); CHECK(r.pv_failed==1&&r.crash_rr_count==2,"task WDT on a valid image counted as crash, not as pending failure");
    CHECK(gw_bootrec_is_crash_rr(5)&&gw_bootrec_is_crash_rr(9)&&!gw_bootrec_is_crash_rr(3)&&!gw_bootrec_is_crash_rr(1),"crash reason classes");
    int n2=gw_bootrec_json(&r,j,sizeof j); CHECK(n2>0&&n2<(int)sizeof j,"json fits"); puts(j); }
  { char bad[GW_VER_MAX]=""; gw_bootrec_t r={0}; char j[256];
    CHECK(!gw_ota_fold_bad(false,false,"2.6.6",bad)&&!bad[0],"normal boot never marks bad");
    CHECK(!gw_ota_fold_bad(true,true,"2.6.6",bad)&&!bad[0],"validated OTA is not bad");
    CHECK(gw_ota_fold_bad(true,false,"2.6.6",bad)&&!strcmp(bad,"2.6.6"),"unvalidated pending boot marks its version bad");
    CHECK(!gw_ota_fold_bad(true,false,"2.6.6",bad),"same bad version: no rewrite");
    CHECK(gw_ota_refuse(bad,"2.6.6")&&!gw_ota_refuse(bad,"2.6.7")&&!gw_ota_refuse("","2.6.6")&&!gw_ota_refuse(NULL,"2.6.6"),"refuse only the failed version");
    CHECK(!gw_ota_fold_bad(true,false,"",bad)&&!strcmp(bad,"2.6.6"),"empty pv_ver ignored");
    char big[64]; memset(big,'9',63); big[63]=0; char b2[GW_VER_MAX]=""; gw_ota_fold_bad(true,false,big,b2); CHECK(strlen(b2)==GW_VER_MAX-1,"long version truncated safely");
    gw_bootrec_on_boot(&r,1,false); int k=gw_bootrec_json_ex(&r,bad,j,sizeof j); CHECK(k>0&&strstr(j,"\"bad_ver\":\"2.6.6\"}")&&k==(int)strlen(j),"bad_ver reported in json");
    gw_bootrec_json_ex(&r,"x\"y",j,sizeof j); CHECK(!strstr(j,"bad_ver"),"unsafe version not injected");
    char tiny[150]; k=gw_bootrec_json_ex(&r,bad,tiny,sizeof tiny); CHECK(tiny[strlen(tiny)-1]=='}',"json stays closed when tight"); puts(j); }
  printf("%d checks, %d failed\n",n,f); return f?1:0; }
