#include "fm1_mdx.h"
#include "fm1_mdx_mix.h"
int16_t fm1_mdx_mix_sample(int32_t fm,int16_t previous,int16_t next,uint32_t phase) {
    int32_t delta=next-previous;
    /* |delta|<=65535 and phase<44100, so the unsigned product fits32 bits.
       Avoid a64-bit divide in the per-sample embedded rendering path. */
    uint32_t distance=(uint32_t)(delta<0?-delta:delta)*phase/FM1_MDX_RATE;
    int32_t pcm=previous+(delta<0?-(int32_t)distance:(int32_t)distance);
    /* PCM is bounded to16 bits. This small rational multiply fits32 bits
       and avoids a software64-bit divide on the embedded sample path. */
    int32_t mixed=fm/2+pcm*FM1_MDX_PCM_GAIN_NUM/FM1_MDX_PCM_GAIN_DEN;
    return (int16_t)(mixed>32767?32767:mixed< -32768?-32768:mixed);
}
