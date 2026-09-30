/* Bounded elastic conversion: USB48kHz <-> DAC44.1kHz. Caller serializes access.
 * Capture taps MDX before the analog master ramp and before PC playback mixing.
 * Linear interpolation is the first profile's conversion quality. */
#include "bridge.h"
static unsigned fill(const fm1_uac_fifo *q){return q->wr-q->rd;}
static void reset(fm1_uac_fifo *q){q->rd=q->wr=q->phase=q->primed=0;}
void fm1_uac_stream(fm1_uac_bridge *b,unsigned direction,int on) {
    if(direction){b->in_active=!!on;reset(&b->capture);}
    else {b->out_active=!!on;reset(&b->playback);}
}
static void push(fm1_uac_fifo *q,int16_t l,int16_t r) {
    if(fill(q)==FM1_UAC_FRAMES){q->rd++;q->overruns++;}
    q->data[q->wr&(FM1_UAC_FRAMES-1)][0]=l;
    q->data[q->wr&(FM1_UAC_FRAMES-1)][1]=r;q->wr++;
}
static void sample(fm1_uac_fifo *q,uint32_t base,int16_t out[2]) {
    unsigned n=fill(q),ch;int correction;
    out[0]=out[1]=0;
    if(!q->primed){if(n<256)return;q->primed=1;}
    if(n<2){q->primed=0;q->underruns++;return;}
    /* Bound adjustment to about1000ppm; queue error closes the clock loop.
       Never change the FM synthesis clock or allocate in an IRQ. */
    correction=(int)n-256;if(correction>128)correction=128;if(correction< -128)correction=-128;
    for(ch=0;ch<2;ch++) {
        int32_t a=q->data[q->rd&(FM1_UAC_FRAMES-1)][ch];
        int32_t d=q->data[(q->rd+1)&(FM1_UAC_FRAMES-1)][ch]-a;
        out[ch]=(int16_t)(a+(d*(int32_t)(q->phase>>1))/32768);
    }
    q->phase+=(uint32_t)((int32_t)base+correction/2);
    q->rd+=q->phase>>16;q->phase&=65535;
}
int fm1_uac_receive(fm1_uac_bridge *b,const uint8_t *p,size_t n) {
    size_t i;
    if(n>FM1_UAC_PACKET || n%4 || (!p && n)){b->bad_packets++;return -1;}
    if(!b->out_active)return 0;
    for(i=0;i<n;i+=4)push(&b->playback,(int16_t)(p[i]|(p[i+1]<<8)),(int16_t)(p[i+2]|(p[i+3]<<8)));
    b->rx_packets++;return 0;
}
static int16_t clip(int32_t v){return (int16_t)(v>32767?32767:v< -32768?-32768:v);}
void fm1_uac_dac(fm1_uac_bridge *b,int32_t *pcm,unsigned n) {
    unsigned i,ch;int16_t pc[2];
    for(i=0;i<n;i++) {
        int16_t l=clip(pcm[2*i]/256),r=clip(pcm[2*i+1]/256);
        if(b->in_active)push(&b->capture,l,r);
        pc[0]=pc[1]=0;
        if(b->out_active)sample(&b->playback,(48000u*65536u)/44100u,pc);
        for(ch=0;ch<2;ch++)pcm[2*i+ch]=(int32_t)clip((ch?r:l)+pc[ch])*256;
    }
}
size_t fm1_uac_transmit(fm1_uac_bridge *b,uint8_t *out,size_t capacity) {
    unsigned i,ch;int16_t v[2];
    if(!b->in_active || !out || capacity<FM1_UAC_PACKET)return 0;
    for(i=0;i<48;i++) {
        sample(&b->capture,(44100u*65536u)/48000u,v);
        for(ch=0;ch<2;ch++){uint16_t s=(uint16_t)v[ch];out[i*4+ch*2]=(uint8_t)s;out[i*4+ch*2+1]=(uint8_t)(s>>8);}
    }
    b->tx_packets++;return FM1_UAC_PACKET;
}
