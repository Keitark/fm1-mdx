#include "fm1_meters.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static fm1_meters m;
static void event_tests(void) {
    fm1_part_motion regular={0},slow,jitter,off,trace={0};fm1_note_motion spectrum={0},split={0};
    uint8_t bands[32];unsigned i;const unsigned intervals[]={17,83,120,280,500};
    fm1_part_step(&regular,1,245,245,1,0);slow=jitter=off=regular;
    for(i=0;i<20;i++)fm1_part_step(&regular,0,0,245,1,50);
    for(i=0;i<12;i++)fm1_part_step(&slow,0,0,245,1,i==11?87:83);
    for(i=0;i<5;i++)fm1_part_step(&jitter,0,0,245,1,intervals[i]);
    CHECK(!memcmp(&regular,&slow,sizeof(regular)) && !memcmp(&regular,&jitter,sizeof(regular)));
    CHECK(regular.step>0 && regular.step<15);
    fm1_part_step(&off,0,0,245,0,1000);CHECK(!off.level_milli);
    /* Source-derived default sensitivity60: 27,27,27,26 over3 ticks. */
    fm1_part_step(&trace,1,245,245,1,0);CHECK(trace.step==27 && trace.counter==60);
    fm1_part_step(&trace,0,0,245,1,17);CHECK(trace.step==27 && trace.counter==33);
    fm1_part_step(&trace,0,0,245,1,17);CHECK(trace.step==27 && trace.counter==6);
    fm1_part_step(&trace,0,0,245,1,16);CHECK(trace.step==26 && trace.counter==39);
    fm1_part_step(&regular,1,36,245,1,50);CHECK(regular.step==4); /* Lower note retriggers. */
    fm1_part_step(&regular,0,0,9,1,0);CHECK(regular.step==1); /* Volume marker limits bar. */
    fm1_part_step(&regular,0,0,245,1,UINT32_MAX);CHECK(!regular.level_milli && !regular.remainder_ms);
    memset(&m,0,sizeof(m));fm1_meters_note(&m,69,127);
    CHECK(m.note_energy[18]==127 && m.note_energy[17]==94 && m.note_energy[16]==39 && m.note_energy[14]==31 && m.note_energy[13]==7);
    fm1_meters_note_spectrum(&m,bands);CHECK(bands[18]==12*255/28);
    fm1_meters_note_spectrum(&m,bands);for(i=0;i<32;i++)CHECK(!bands[i]);
    for(i=0;i<10000;i++)fm1_meters_note(&m,69,127);
    CHECK(m.note_energy[18]==65535);fm1_meters_note_spectrum(&m,bands);CHECK(bands[18]==255 && !bands[0] && !bands[31]);
    fm1_meters_note(&m,13,127);fm1_meters_note(&m,108,127);fm1_meters_note_spectrum(&m,bands);
    CHECK(bands[0]==12*255/28 && bands[31]==12*255/28);
    memset(&m,0,sizeof(m));fm1_meters_note(&m,0,127);fm1_meters_note(&m,UINT32_MAX,127);fm1_meters_note(&m,69,0);
    for(i=0;i<32;i++)CHECK(!m.note_energy[i]);
    fm1_note_step(&spectrum,127,17);CHECK(spectrum.step==6 && spectrum.peak==6 && spectrum.peak_count==59);
    fm1_note_step(&spectrum,0,17);CHECK(spectrum.step==9);
    fm1_note_step(&spectrum,0,16);CHECK(spectrum.step==11);
    split=spectrum;
    fm1_note_step(&spectrum,0,1000);for(i=0;i<20;i++)fm1_note_step(&split,0,50);
    CHECK(!memcmp(&spectrum,&split,sizeof(spectrum)));
    fm1_note_step(&spectrum,0,UINT32_MAX);CHECK(!spectrum.step && !spectrum.peak);
    fm1_note_step(&spectrum,65535,84);CHECK(spectrum.step==28 && spectrum.peak==28 && spectrum.peak_count==59);
    fm1_note_step(&spectrum,0,983);CHECK(spectrum.step && spectrum.peak==28 && !spectrum.peak_count);
    fm1_note_step(&spectrum,0,17);CHECK(spectrum.peak==27 && spectrum.peak_count==5);
    fm1_note_step(&spectrum,0,100);CHECK(spectrum.peak==26); /*6-tick fall with nonzero bar. */
    spectrum.step=spectrum.target=spectrum.counter=0;spectrum.peak_count=0;
    fm1_note_step(&spectrum,0,17);CHECK(spectrum.peak==25 && spectrum.peak_count==1);
    fm1_note_step(&spectrum,0,33);CHECK(spectrum.peak==24); /*2-tick fall with zero bar. */
}
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
    motion_tests();event_tests();
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
