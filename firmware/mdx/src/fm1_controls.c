#include "fm1_controls.h"
int64_t fm1_selector_step(fm1_selector *s,int32_t count) {
    int64_t edges=(int64_t)count-s->observed+s->partial;
    int64_t steps=edges/2;
    s->observed=count;s->partial=(int8_t)(edges%2);
    return steps;
}
