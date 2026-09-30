#include "bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static fm1_uac_bridge b;
static void packet(uint8_t *p,int16_t l,int16_t r) {
    unsigned i;for(i=0;i<48;i++){p[4*i]=(uint8_t)l;p[4*i+1]=(uint16_t)l>>8;p[4*i+2]=(uint8_t)r;p[4*i+3]=(uint16_t)r>>8;}
}
static int16_t word(const uint8_t *p){return (int16_t)(p[0]|(p[1]<<8));}
static void run(int ppm) {
    uint8_t in[192],out[192];int32_t pcm[2*46];unsigned ms,i,n;uint64_t clock=0;
    memset(&b,0,sizeof(b));fm1_uac_stream(&b,0,1);fm1_uac_stream(&b,1,1);packet(in,6000,-6000);
    for(ms=0;ms<60000;ms++) {
        CHECK(!fm1_uac_receive(&b,in,sizeof(in)));
        clock+=(uint64_t)44100*(1000000+ppm);n=(unsigned)(clock/1000000000);clock%=1000000000;
        CHECK(n<=46);
        for(i=0;i<n;i++){pcm[2*i]=10000*256;pcm[2*i+1]=-10000*256;}
        fm1_uac_dac(&b,pcm,n);CHECK(fm1_uac_transmit(&b,out,sizeof(out))==192);
        if(ms>1000) {
            CHECK(pcm[0]==16000*256 && pcm[1]==-16000*256);
            CHECK(word(out)==10000 && word(out+2)==-10000); /* no PC loopback */
        }
    }
    CHECK(!b.capture.underruns && !b.playback.underruns && !b.capture.overruns && !b.playback.overruns);
    CHECK(b.capture.wr-b.capture.rd<1024 && b.playback.wr-b.playback.rd<1024);
}
int main(int argc,char **argv) {
    unsigned i,offset=0;uint8_t p[192];int32_t pcm[128];
    if(argc>1 && !strcmp(argv[1],"--descriptor")){for(i=0;i<173;i++)printf("%02x",fm1_uac_descriptor[i]);puts("");return 0;}
    while(offset<173){unsigned n=fm1_uac_descriptor[offset];CHECK(n>=2 && n<=173-offset);offset+=n;}CHECK(offset==173);
    CHECK(fm1_uac_interface_request(1,11,1,3,0));CHECK(fm1_uac_interface_request(0x81,10,0,4,1));
    CHECK(!fm1_uac_interface_request(1,11,1,2,0));CHECK(!fm1_uac_interface_request(1,11,2,4,0));
    CHECK(!fm1_uac_interface_request(0x21,11,1,3,0));CHECK(!fm1_uac_interface_request(1,11,1,0x103,0));
    CHECK(!fm1_uac_interface_request(1,11,1,3,65535));CHECK(!fm1_uac_interface_request(0x21,1,0,3,3));
    run(0);run(500);run(-500);
    CHECK(fm1_uac_receive(&b,p,193)==-1 && fm1_uac_receive(&b,p,3)==-1);
    CHECK(fm1_uac_receive(&b,NULL,4)==-1 && b.bad_packets==3);
    packet(p,32767,-32768);fm1_uac_stream(&b,0,1);
    for(i=0;i<20;i++)fm1_uac_receive(&b,p,192);
    for(i=0;i<64;i++){pcm[2*i]=32767*256;pcm[2*i+1]=-32768*256;}
    fm1_uac_dac(&b,pcm,64);CHECK(pcm[0]==32767*256 && pcm[1]==-32768*256);
    fm1_uac_stream(&b,0,0);fm1_uac_stream(&b,1,0);
    CHECK(fm1_uac_transmit(&b,p,192)==0 && !b.playback.primed && b.playback.wr==b.playback.rd);
    fm1_uac_stream(&b,0,1);memset(pcm,0,sizeof(pcm));fm1_uac_dac(&b,pcm,64);CHECK(pcm[0]==0);
    puts("PASS duplex48k bridge, stereo/loopback isolation, +/-500ppm clock drift, malformed packets, clipping and reset");return 0;
}
