/* Integer form of mdxtools' cubic output curve. */
#ifndef FM1_OPM_OUTPUT_H
#define FM1_OPM_OUTPUT_H
#include <stdint.h>
static inline int32_t fm1_opm_output(int32_t sample) {
    uint32_t magnitude;uint64_t cube;uint32_t correction;int negative=sample<0;
    /* Retain the upstream out-of-range behavior for this scoped optimization. */
    if(sample < -32767 || sample > 32767)return 32767;
    magnitude=(uint32_t)(sample<0?-sample:sample);
    cube=(uint64_t)magnitude*magnitude*magnitude;
    /* x - x^3/(3*32767^2), followed by truncation toward zero.
       Ceil the positive correction to preserve that final truncation. */
    correction=(uint32_t)((cube+UINT64_C(3221028866))/UINT64_C(3221028867));
    sample=(int32_t)(magnitude-correction);
    return negative?-sample:sample;
}
#endif
