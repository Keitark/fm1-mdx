#ifndef FM1_MDX_MIX_H
#define FM1_MDX_MIX_H
#include <stdint.h>
#define FM1_MDX_PCM_GAIN_NUM 8
#define FM1_MDX_PCM_GAIN_DEN 5
/* Native MXDRV/X68Sound OutRun stems show a6.469dB PCM/FM deficit with
   the preceding49700/65536 gain. PCM gain8/5 corrects it within0.02dB
   while retaining FM gain1/2. Different decoders/filters remain distinct.
   phase is the native PCM remainder:0<=phase<FM1_MDX_RATE. */
int16_t fm1_mdx_mix_sample(int32_t fm,int16_t previous,int16_t next,uint32_t phase);
#endif
