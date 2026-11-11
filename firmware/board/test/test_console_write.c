#include "../gw_console_write.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static char output[4096]; static size_t used, calls; static int mode;
static int send(const char* s,size_t n){
 assert(n<=128); ++calls;
 if(mode==1) return 0;
 if(mode==2 && n>17) n=17;
 if(mode==3) return -1;
 if(mode==4) return (int)n+1;
 memcpy(output+used,s,n); used+=n; return (int)n;
}
int main(void){
 char input[4096]; for(size_t i=0;i<sizeof input;i++) input[i]=(char)(i%113+1);
 const size_t sizes[]={0,1,127,128,129,255,256,257,4095,4096};
 for(int partial=0;partial<2;partial++) for(size_t i=0;i<10;i++){
  mode=partial?2:0; used=calls=0;
  assert(gw_console_write(input,sizes[i],send));
  assert(used==sizes[i] && !memcmp(input,output,used));
 }
 for(mode=1;mode<=4;mode++){ if(mode==2) continue; used=calls=0;
  assert(!gw_console_write(input,4096,send)); assert(calls==1 && used==0);
 }
 puts("Console transport: 23 boundary/partial/error cases passed");
}
