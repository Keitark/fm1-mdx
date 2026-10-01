#include "fm1_mdx.h"
#include "fm1_mdx_usb.h"
#include "fm1_mdx_mix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
extern const unsigned char fm1_demo_mdx[],fm1_demo_pdx[];
extern const size_t fm1_demo_mdx_size,fm1_demo_pdx_size;
static fm1_mdx_player p;
static fm1_mdx_upload upload;
static int16_t stereo[256];
static char response[200];
static unsigned requested;
static int idle(void *ctx){(void)ctx;return 1;}
static int request(void *ctx,unsigned op,unsigned a,unsigned b){(void)ctx;(void)a;(void)b;requested=op;return 0;}
static void status(void *ctx,char *out,size_t n){(void)ctx;snprintf(out,n,"MDX running=0\n");}
static void reply(void *ctx,const char *s){(void)ctx;snprintf(response,sizeof(response),"%s",s);}
static void line(const char *s,unsigned now){fm1_mdx_usb_io io={0,idle,request,status};CHECK(fm1_mdx_usb_line(&upload,&io,s,now,reply,0));}
static unsigned crc32(const uint8_t *b,size_t n) {
    unsigned c=~0u,i;while(n--){c^=*b++;for(i=0;i<8;i++)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;
}
static long long audio(unsigned blocks){unsigned i,j;long long sum=0;for(i=0;i<blocks;i++){CHECK(!fm1_mdx_render(&p,stereo,128));for(j=0;j<256;j++)sum+=abs(stereo[j]);}return sum;}
static void activity_tests(void) {
    uint8_t volume[16],onset[16],bands[32];uint16_t held;
    CHECK(!fm1_mdx_load(&p,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    p.sequence.tracks[0].opm_volume=0;
    fm1_mdx_song_write(&p,0x28,0x4a);fm1_mdx_song_write(&p,8,0x78);fm1_mdx_song_write(&p,8,0);
    /* An entire short note between display views still generates one pulse. */
    CHECK(fm1_mdx_part_activity(&p,volume,onset,&held)==1 && !held);
    CHECK(volume[0]==245 && onset[0]==245);
    CHECK(!fm1_mdx_part_activity(&p,volume,onset,&held));
    fm1_meters_note_spectrum(&p.meters,bands);CHECK(bands[18]==12*255/28);
    CHECK(!fm1_mdx_mute(&p,0,1));
    fm1_mdx_song_write(&p,8,0x78);fm1_mdx_song_write(&p,0x40,0x32);
    CHECK(!fm1_mdx_part_activity(&p,volume,onset,&held) && !held && p.registers[0x40]==0x32);
    CHECK(!fm1_mdx_note(&p,69,1));
    CHECK(fm1_mdx_part_activity(&p,volume,onset,&held)==1 && (held&1));
    CHECK(!fm1_mdx_note(&p,70,0));CHECK(!fm1_mdx_part_activity(&p,volume,onset,&held) && (held&1));
    CHECK(!fm1_mdx_note(&p,69,0));CHECK(!fm1_mdx_part_activity(&p,volume,onset,&held) && !held);
    p.meter_seen|=0x100;p.pcm.voices[0].active=1;
    CHECK(!fm1_mdx_part_activity(&p,volume,onset,&held) && (held&0x100));
    CHECK(!retrofm_pcm_stop(&p.pcm,0));CHECK(!fm1_mdx_part_activity(&p,volume,onset,&held) && !held);
    fm1_mdx_stop(&p);CHECK(!p.meter_seen && !p.meter_triggers);
}
int main(void) {
    unsigned i,n;uint8_t image[4096];char s[300];activity_tests();
    line("MDX GUIDE 1",0);CHECK(requested==FM1_MDX_GUIDE && !strncmp(response,"OK",2));
    line("MDX GUIDE 0",0);CHECK(requested==FM1_MDX_GUIDE && !strncmp(response,"OK",2));
    line("MDX GUIDE 2",0);CHECK(!strncmp(response,"ERR",3));
    /* A linear source must remain linear at fractional sample positions.
       Also exercise negative full-scale differences without signed overflow. */
    CHECK(fm1_mdx_mix_sample(0,0,10000,0)==0);
    CHECK(fm1_mdx_mix_sample(0,0,10000,FM1_MDX_RATE/2)==8000);
    CHECK(fm1_mdx_mix_sample(0,10000,0,FM1_MDX_RATE/2)==8000);
    CHECK(fm1_mdx_mix_sample(0,-32768,32767,FM1_MDX_RATE/2)==-1);
    CHECK(fm1_mdx_mix_sample(0,-32767,32767,FM1_MDX_RATE/2)==0);
    CHECK(fm1_mdx_mix_sample(20000,0,0,0)==10000);
    CHECK(fm1_mdx_mix_sample(32767,32767,32767,0)==32767);
    CHECK(fm1_mdx_mix_sample(-32768,-32768,-32768,0)==-32768);
    CHECK(fm1_mdx_mix_sample(0,10000,10000,0)==16000);
    CHECK(fm1_mdx_mix_sample(0,-10000,-10000,0)==-16000);
    CHECK(fm1_mdx_mix_sample(0,32767,32767,0)==32767);
    CHECK(fm1_mdx_mix_sample(0,-32768,-32768,0)==-32768);
    CHECK(!fm1_mdx_load(&p,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    CHECK(p.mdx.track_count==9);CHECK(audio(400)>100000);
    CHECK((p.meter_seen&0x101)==0x101);
    CHECK(!p.meters.count); /* No target FFT sampling. */
    CHECK(fm1_mdx_mute(&p,16,1)==-1);
    CHECK(!fm1_mdx_mute(&p,0,1));
    fm1_mdx_song_write(&p,8,0x78);CHECK(p.registers[8]==0);
    fm1_mdx_song_write(&p,0x40,0x21);CHECK(p.registers[0x40]==0x21);
    CHECK(!fm1_mdx_note(&p,60,1));CHECK(p.registers[8]==0x78);
    fm1_mdx_song_write(&p,8,0);CHECK(p.registers[8]==0x78);
    CHECK(audio(100)>0);CHECK(p.live_note[0]==60);CHECK(p.suppressed_keys>1);
    {unsigned seen=0,j;for(j=0;j<1200;j++) {
        CHECK(!fm1_mdx_render(&p,stereo,128));CHECK(p.live_note[0]==60);
        CHECK(p.opm.oper[0].key);CHECK(p.registers[0x28]==0x3e);
        if(p.sequence.tracks[0].voice_number>=0)seen|=1u<<p.sequence.tracks[0].voice_number;
        CHECK(p.registers[0x20]==p.song_registers[0x20]);
    }CHECK((seen&3)==3);}
    CHECK(!fm1_mdx_note(&p,61,0));CHECK(p.live_note[0]==60);
    CHECK(!fm1_mdx_note(&p,60,0));CHECK(p.live_note[0]==-1);CHECK(p.registers[8]==0);
    CHECK(!fm1_mdx_note(&p,60,1));CHECK(!fm1_mdx_select(&p,1));CHECK(p.live_note[0]==-1);
    CHECK(fm1_mdx_note(&p,60,1)==-1);CHECK(!fm1_mdx_mute(&p,1,1));CHECK(!fm1_mdx_note(&p,60,1));
    CHECK(!fm1_mdx_mute(&p,1,0));CHECK(p.live_note[1]==-1);
    for(i=0;i<9;i++)CHECK(!fm1_mdx_mute(&p,i,1));
    audio(400);memset(&p.meters,0,sizeof(p.meters));CHECK(audio(100)==0);
    p.meter_triggers=0;audio(20);CHECK(!p.meter_triggers); /* Muted song emits no meter onset. */
    CHECK(!fm1_mdx_select(&p,0));CHECK(!fm1_mdx_note(&p,65,1));CHECK(audio(20)>0);CHECK(p.meter_triggers&1);
    CHECK(p.registers[0x40]==p.song_registers[0x40]);
    fm1_mdx_stop(&p);CHECK(!p.playing);for(i=0;i<8;i++)CHECK(p.live_note[i]==-1);
    CHECK(fm1_mdx_load(&p,fm1_demo_mdx,fm1_demo_mdx_size,0,0)==RETROFM_MDX_MISSING_PDX);
    memcpy(image,"FM1M",4);n=(unsigned)fm1_demo_mdx_size;memcpy(image+4,&n,4);n=(unsigned)fm1_demo_pdx_size;memcpy(image+8,&n,4);
    memcpy(image+12,fm1_demo_mdx,fm1_demo_mdx_size);memcpy(image+12+fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size);
    n=(unsigned)(12+fm1_demo_mdx_size+fm1_demo_pdx_size);
    snprintf(s,sizeof(s),"MDX BEGIN %08x %08x",n,crc32(image,n));line(s,0);CHECK(upload.active);
    for(i=0;i<n;) {unsigned j,count=n-i;if(count>120)count=120;snprintf(s,sizeof(s),"MDX DATA %08x ",i);for(j=0;j<count;j++)snprintf(s+18+j*2,3,"%02x",image[i+j]);line(s,1);i+=count;CHECK(upload.received==i);}
    line("MDX END",2);CHECK(upload.ready && !upload.active);CHECK(upload.mdx_size==fm1_demo_mdx_size);
    line("MDX PLAY",3);CHECK(requested==FM1_MDX_PLAY);
    line("MDX MUTE 0F 1",3);CHECK(requested==FM1_MDX_MUTE);
    line("MDX NOTE 3C 1",3);CHECK(requested==FM1_MDX_NOTE);
    line("MDX BEGIN 00000010 00000000",4);line("MDX DATA 00000001 00",5);CHECK(!upload.active && !upload.ready);
    line("MDX BEGIN 00000010 00000000",6);fm1_mdx_usb_timeout(&upload,10007);CHECK(!upload.active);
    line("MDX BEGIN FFFFFFFF 00000000",8);CHECK(!upload.active);
    /* Truncated headers and every single-byte mutation of the demo must remain bounded. */
    for(i=0;i<fm1_demo_mdx_size;i++) {
        memcpy(image,fm1_demo_mdx,fm1_demo_mdx_size);image[i]^=0xff;
        if(!fm1_mdx_load(&p,image,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size)){
            unsigned j;for(j=0;j<5;j++)if(fm1_mdx_render(&p,stereo,128))break;
        }
    }
    for(i=0;i<40;i++)CHECK(fm1_mdx_load(&p,fm1_demo_mdx,i,0,0));
    puts("PASS stereo MDX/PDX, live ownership, parameter continuity, mute transitions, bounded USB and malformed files");return 0;
}
