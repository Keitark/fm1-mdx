#include "fm1_meters.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static fm1_meters m;
static void motion_tests(void) {
    fm1_meter_motion regular={0},slow={0},jitter={0},cap={0};
    const unsigned intervals[]={17,83,120,280,500};unsigned i;
    fm1_meter_step(&regular,255,0,255,400,85);slow=jitter=regular;
    CHECK(regular.level_milli==255000 && regular.peak_milli==255000 && regular.hold_ms==400);
    for(i=0;i<20;i++)fm1_meter_step(&regular,0,50,255,400,85);
    for(i=0;i<12;i++)fm1_meter_step(&slow,0,i==11?87:83,255,400,85);
    for(i=0;i<5;i++)fm1_meter_step(&jitter,0,intervals[i],255,400,85);
    CHECK(!memcmp(&regular,&slow,sizeof(regular)) && !memcmp(&regular,&jitter,sizeof(regular)));
    CHECK(!regular.level_milli && regular.peak_milli==204000 && !regular.hold_ms);
    fm1_meter_step(&cap,255,0,255,400,85);
    fm1_meter_step(&cap,100,300,255,400,85);CHECK(cap.hold_ms==100 && cap.peak_milli==255000);
    fm1_meter_step(&cap,0,100,255,400,85);CHECK(!cap.hold_ms && cap.peak_milli==255000);
    fm1_meter_step(&cap,0,200,255,400,85);CHECK(cap.peak_milli==238000);
    fm1_meter_step(&cap,250,50,255,400,85);CHECK(cap.level_milli==250000 && cap.peak_milli==250000 && cap.hold_ms==400);
    fm1_meter_step(&cap,0,UINT32_MAX,255,400,85);CHECK(!cap.level_milli && !cap.peak_milli && !cap.hold_ms);
    /* FFT and stereo tails have their own rates, with immediate new attack. */
    memset(&cap,0,sizeof(cap));fm1_meter_step(&cap,200,0,136,0,136);
    fm1_meter_step(&cap,0,500,136,0,136);CHECK(cap.level_milli==132000);
    fm1_meter_step(&cap,240,50,136,0,136);CHECK(cap.level_milli==240000);
    memset(&cap,0,sizeof(cap));fm1_meter_step(&cap,200,0,170,0,170);
    fm1_meter_step(&cap,0,500,170,0,170);CHECK(cap.level_milli==115000);
}
static void tone(int bin,int mode,uint8_t *out) {
    unsigned i;memset(&m,0,sizeof(m));
    for(i=0;i<256;i++){int16_t v=(int16_t)(20000*sin(6.283185307179586*bin*i/256));fm1_meters_feed(&m,v,mode==0?0:mode==1?v:-v);}
    fm1_meters_spectrum(&m,out);CHECK(!m.count);
}
int main(void) {
    uint8_t out[24]={0},anti[24],left[24];unsigned i;
    motion_tests();
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
    puts("PASS stereo FFT and elapsed-time envelopes: attack, release, hold, retrigger, irregular FPS and long pause");return 0;
}
