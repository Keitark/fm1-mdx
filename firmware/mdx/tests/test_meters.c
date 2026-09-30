#include "fm1_meters.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static fm1_meters m;
static void tone(int bin,int mode,uint8_t *out) {
    unsigned i;memset(&m,0,sizeof(m));
    for(i=0;i<256;i++){int16_t v=(int16_t)(20000*sin(6.283185307179586*bin*i/256));fm1_meters_feed(&m,v,mode==0?0:mode==1?v:-v);}
    fm1_meters_spectrum(&m,out);CHECK(!m.count);
}
int main(void) {
    uint8_t out[24]={0},anti[24],left[24];unsigned i;
    fm1_meters_spectrum(&m,out);for(i=0;i<24;i++)CHECK(!out[i]);
    for(i=0;i<256;i++)fm1_meters_feed(&m,0,0);
    fm1_meters_spectrum(&m,out);for(i=0;i<24;i++)CHECK(!out[i]);
    tone(10,1,out);tone(10,2,anti);tone(10,0,left);
    CHECK(out[8]>220);for(i=0;i<24;i++){CHECK(abs(out[i]-anti[i])<=2);CHECK(out[i]==left[i]);if(i!=7&&i!=8&&i!=9)CHECK(out[i]<out[8]);}
    tone(80,1,out);CHECK(out[19]>220 && out[8]<30);
    memset(&m,0,sizeof(m));for(i=0;i<256;i++)fm1_meters_feed(&m,(i&1)?32767:-32768,(i&1)?-32768:32767);
    fm1_meters_spectrum(&m,out);CHECK(out[23]>240);
    memset(&m,0,sizeof(m));for(i=0;i<256;i++)fm1_meters_feed(&m,(i&1)?10000:-10000,0);
    fm1_meters_spectrum(&m,out);CHECK(abs(out[23]-fm1_meters_level(10000))<=2);
    CHECK(fm1_meters_level(0)==0 && fm1_meters_level(32)==0 && fm1_meters_level(32768)==255);
    CHECK(fm1_meters_level(16384)>fm1_meters_level(8192));
    fm1_meters_peak(&m.parts[0],-32768);CHECK(m.parts[0]==32768);
    puts("PASS fixed-point stereo FFT: silence, frequency, phase, full-scale and peaks");return 0;
}
