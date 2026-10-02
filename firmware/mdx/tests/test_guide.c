#include "fm1_guide.h"
#include "fm1_controls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
extern const unsigned char fm1_demo_mdx[],fm1_demo_pdx[];
extern const size_t fm1_demo_mdx_size,fm1_demo_pdx_size;
static fm1_mdx_player p,baseline,saved;
static fm1_guide g;
static retrofm_mdx_sequencer reference;
static uint64_t due[64];static uint8_t notes[64];static unsigned count;
static bool pcm(void *u,const retrofm_mdx_pcm_command *c){(void)u;(void)c;return true;}
static bool trace(void *u,const retrofm_event *e){(void)u;
    if(e->opcode==RETROFM_OP_YM2151 && e->reg==8 && (e->data&0x78) && !(e->data&7) && count<64){due[count]=reference.current_cycles;notes[count++]=(uint8_t)(reference.tracks[0].note+13);}
    return true;
}
static void selectors(void){
    fm1_selector s={0};
    CHECK(!fm1_selector_step(&s,1));CHECK(fm1_selector_step(&s,2)==1);
    CHECK(!fm1_selector_step(&s,3));CHECK(!fm1_selector_step(&s,2)); /* cancel reversal */
    CHECK(!fm1_selector_step(&s,1));CHECK(fm1_selector_step(&s,0)==-1);
    CHECK(fm1_selector_step(&s,6)==3);CHECK(fm1_selector_step(&s,-2)==-4);
    CHECK(!fm1_selector_step(&s,-2));CHECK(!fm1_selector_step(&s,-3));CHECK(fm1_selector_step(&s,-4)==-1);
    s.observed=2147483646;s.partial=0;CHECK(!fm1_selector_step(&s,2147483647));
}
static void keyboard(void){
    int dir;unsigned n;g.active=1;g.count=1;g.head=0;
    for(n=17;n<=103;n++){
        unsigned slot;g.note[0]=(uint8_t)n;slot=fm1_guide_key(&g,0,&dir);
        CHECK(slot>=14 && slot<=40);
        CHECK(dir==(n<53?-1:n>79?1:0));
        if(!dir)CHECK(slot==n-53+14);
    }
    g.note[0]=16;CHECK(fm1_guide_key(&g,0,&dir)==41 && !dir);
    g.note[0]=104;CHECK(fm1_guide_key(&g,0,&dir)==41 && !dir);
    g.note[0]=41;CHECK(fm1_guide_key(&g,-1,&dir)==14 && !dir);
    g.note[0]=60;CHECK(fm1_guide_key(&g,0,&dir)==21 && !dir);
    g.active=0;CHECK(fm1_guide_key(&g,0,&dir)==41);
}
static void preview(void){
    unsigned i;int16_t a[256],b[256];
    CHECK(!fm1_mdx_load(&p,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    fm1_guide_start(&g,&p);CHECK(!g.active); /* Only muted FM parts. */
    CHECK(!fm1_mdx_mute(&p,0,1));p.sequence.tracks[0].key_on_delay=3;
    saved=p;fm1_guide_start(&g,&p);CHECK(g.active);
    reference=p.sequence;reference.event_callback=trace;reference.callback_user=0;reference.pcm_callback=pcm;reference.pcm_callback_user=0;
    count=0;for(i=0;i<450;i++)CHECK(!retrofm_mdx_sequencer_tick(&reference));
    CHECK(count>10 && notes[0]==60 && notes[1]==64 && notes[8]==60); /* Loop repeats. */
    for(i=0;i<64 && g.count<2;i++)fm1_guide_step(&g,0,2);
    CHECK(!memcmp(&p,&saved,sizeof(p))); /* No live state/patch/output mutations. */
    CHECK(g.count>=2 && g.due[g.head]==due[0] && fm1_guide_note(&g)==notes[0]);
    CHECK(!fm1_guide_hit(&g,61) && !g.hits && g.count>=2);
    CHECK(fm1_guide_hit(&g,60) && g.hits==1 && fm1_guide_note(&g)==64);
    fm1_guide_step(&g,due[1],2);CHECK(g.missed==1);
    for(i=0;i<128 && !g.count;i++)fm1_guide_step(&g,due[1],2);
    CHECK(fm1_guide_note(&g)==notes[2]);
    for(i=2;i<14;i++){
        unsigned k;for(k=0;k<128 && g.count<2;k++)fm1_guide_step(&g,due[i-1],2);
        CHECK(g.count>=2 && fm1_guide_note(&g)==notes[i] && g.due[g.head]==due[i]);
        fm1_guide_step(&g,due[i],2);
    }
    /* Rendering with preview work is sample-identical to no preview work. */
    CHECK(!fm1_mdx_load(&baseline,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    CHECK(!fm1_mdx_load(&p,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    fm1_mdx_mute(&p,0,1);fm1_mdx_mute(&baseline,0,1);fm1_guide_start(&g,&p);
    for(i=0;i<4000;i++){
        CHECK(!fm1_mdx_render(&p,a,128));CHECK(!fm1_mdx_render(&baseline,b,128));
        fm1_guide_step(&g,p.cycles,2);CHECK(!g.error);CHECK(!memcmp(a,b,sizeof(a)));
    }
    fm1_guide_stop(&g);CHECK(!g.count && fm1_guide_note(&g)==-1);
}
static void timing_commands(void){
    /* Wait for another track, rest, two tempo changes, and a tied note that
       changes pitch without another key-on. Actual playback supplies the trace. */
    static const uint8_t melody[]={0xfd,0,0xee,9,0xff,180,175,7,0xff,220,179,4,0xf7,182,3,184,3,0xf1,0};
    static const uint8_t sync[]={0xfd,0,3,0xef,0,0xf1,0};
    unsigned i;
    CHECK(!fm1_mdx_load(&p,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    fm1_mdx_mute(&p,0,1);
    for(i=0;i<16;i++){p.sequence.tracks[i].used=false;p.sequence.tracks[i].ended=true;}
    p.sequence.tracks[0].used=p.sequence.tracks[1].used=true;p.sequence.tracks[0].ended=p.sequence.tracks[1].ended=false;
    p.sequence.tracks[0].data=melody;p.sequence.tracks[0].size=sizeof(melody);
    p.sequence.tracks[1].data=sync;p.sequence.tracks[1].size=sizeof(sync);
    fm1_guide_start(&g,&p);reference=p.sequence;reference.event_callback=trace;reference.callback_user=0;reference.pcm_callback=pcm;reference.pcm_callback_user=0;
    count=0;for(i=0;i<100 && !reference.ended;i++)CHECK(!retrofm_mdx_sequencer_tick(&reference));
    CHECK(reference.ended && count==3 && notes[0]==60 && notes[1]==64 && notes[2]==67);
    for(i=0;i<3;i++){
        unsigned k;for(k=0;k<100 && !g.count;k++)fm1_guide_step(&g,i?due[i-1]:0,2);
        CHECK(g.count && fm1_guide_note(&g)==notes[i] && g.due[g.head]==due[i]);
        fm1_guide_step(&g,due[i],0); /* Expiry itself never runs lookahead. */
    }
    for(i=0;i<100 && !g.preview.ended;i++)fm1_guide_step(&g,due[2],2);
    CHECK(g.preview.ended && !g.count && !g.error);
}
int main(void){selectors();keyboard();preview();timing_commands();puts("PASS detents, partial reversal, burst motion, guide lookahead, exact pitches/timestamps, early hits, wrong keys, timed expiry, loops, synchronization, rests, tempo, ties, delayed key-on and unchanged audio");return 0;}
