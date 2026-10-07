#include "fm1_guide.h"
#include <string.h>
static bool preview_pcm(void *u,const retrofm_mdx_pcm_command *c){(void)u;(void)c;return true;}
static int expired(const fm1_guide *g,uint64_t due,uint64_t now) {
    return due<=now && (g->mode!=FM1_GUIDE_TIMING || now-due>=FM1_GUIDE_TIMING_LATE);
}
static bool preview_event(void *u,const retrofm_event *e) {
    fm1_guide *g=u;
    if(e->opcode==RETROFM_OP_YM2151 && e->reg==8 && (e->data&0x78) && (e->data&7)==g->selected) {
        int midi=g->preview.tracks[g->selected].note+13;
        unsigned at=(g->head+g->count)%FM1_GUIDE_QUEUE;
        if(midi<13 || midi>108)return true; /* Raw register triggers have no score pitch. */
        if(expired(g,g->preview.current_cycles,g->now))return true;
        if(g->count==FM1_GUIDE_QUEUE){g->error=1;return false;}
        g->due[at]=g->preview.current_cycles;g->note[at]=(uint8_t)midi;g->count++;
    }
    return true;
}
void fm1_guide_stop(fm1_guide *g){g->active=g->count=0;}
void fm1_guide_start(fm1_guide *g,const fm1_mdx_player *p) {
    fm1_guide_start_mode(g,p,FM1_GUIDE_NOTE);
}
void fm1_guide_start_mode(fm1_guide *g,const fm1_mdx_player *p,unsigned mode) {
    memset(g,0,sizeof(*g));
    if(!p->playing || p->selected>=8 || !(p->mute_mask&(1u<<p->selected)) || mode<FM1_GUIDE_NOTE || mode>FM1_GUIDE_TIMING)return;
    g->mode=(uint8_t)mode;
    g->preview=p->sequence;g->selected=p->selected;g->active=1;
    g->preview.event_callback=preview_event;g->preview.callback_user=g;
    g->preview.pcm_callback=preview_pcm;g->preview.pcm_callback_user=g;
    /* Driver methods recover their owner by offsetof, with no live pointers. */
}
static void pop(fm1_guide *g){g->head=(g->head+1)%FM1_GUIDE_QUEUE;g->count--;}
void fm1_guide_step(fm1_guide *g,uint64_t now,unsigned budget) {
    if(!g->active)return;g->now=now;
    while(g->count && expired(g,g->due[g->head],now)){pop(g);g->missed++;}
    while(budget-- && g->count<2 && !g->preview.ended && !g->error) {
        int r=retrofm_mdx_sequencer_tick(&g->preview);
        if(r && r!=RETROFM_MDX_END)g->error=r;
    }
    if(g->error)g->count=0;
}
int fm1_guide_hit(fm1_guide *g,unsigned midi) {
    if(!g->active || !g->count || g->note[g->head]!=midi)return 0;
    pop(g);g->hits++;return 1;
}
int fm1_guide_note(const fm1_guide *g){return g->active && g->count?g->note[g->head]:-1;}
unsigned fm1_guide_key(const fm1_guide *g,int octave,int *direction) {
    int midi=fm1_guide_note(g),low=53+12*octave,base=octave,slot;
    *direction=0;if(midi<0)return 41;
    /* Minimal octave movement retains the preferred physical key position. */
    while(midi<53+12*base && base>-3)base--;
    while(midi>79+12*base && base<2)base++;
    slot=midi-(53+12*base)+14;
    if(slot<14 || slot>40)return 41; /* Outside the board's MIDI17..103 range. */
    if(midi<low)*direction=-1;else if(midi>low+26)*direction=1;
    return (unsigned)slot;
}
unsigned fm1_guide_progress(const fm1_guide *g) {
    uint64_t remaining;
    if(!g->active || !g->count)return 0;
    if(g->due[g->head]<=g->now)return 255;
    remaining=g->due[g->head]-g->now;
    if(remaining>=FM1_GUIDE_TIMING_LEAD)return 0;
    return (unsigned)((FM1_GUIDE_TIMING_LEAD-remaining)*255/FM1_GUIDE_TIMING_LEAD);
}
uint64_t fm1_guide_lights(const fm1_guide *g,int octave) {
    int direction;unsigned slot;uint64_t lights=0;
    if(g->mode==FM1_GUIDE_TIMING) {
        if(g->active && g->count && (g->due[g->head]<=g->now || g->due[g->head]-g->now<=FM1_GUIDE_TIMING_PULSE))return FM1_GUIDE_NOTE_LIGHTS;
        return 0;
    }
    slot=fm1_guide_key(g,octave,&direction);
    if(slot<41)lights|=UINT64_C(1)<<slot;
    if(direction)lights|=UINT64_C(1)<<(direction>0?1:0);
    return lights;
}
