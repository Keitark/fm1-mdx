#ifndef FM1_MDX_MIX_H
#define FM1_MDX_MIX_H
#include <stdint.h>
#define FM1_MDX_PCM_BALANCE_Q15 49700
/* RetroFM's measured PCM/FM ratio, with the existing 1/2 common headroom.
   X68Sound's separate PCM trim is not applicable to this decoder.
   phase is the native PCM remainder:0<=phase<FM1_MDX_RATE. */
int16_t fm1_mdx_mix_sample(int32_t fm,int16_t previous,int16_t next,uint32_t phase);
#endif
