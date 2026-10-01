#ifndef FM1_CONTROLS_H
#define FM1_CONTROLS_H
#include <stdint.h>
/* SELECT reports two quadrature contact edges per physical detent on this unit.
   Retain partial motion; reversal cancels it instead of causing another step. */
typedef struct { int32_t observed; int8_t partial; } fm1_selector;
int64_t fm1_selector_step(fm1_selector *,int32_t count);
#endif
