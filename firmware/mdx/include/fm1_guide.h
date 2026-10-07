#ifndef FM1_GUIDE_H
#define FM1_GUIDE_H
#include "fm1_mdx.h"
/* A silent sequencing copy, never a second synth. Bounded cooperative work,
   with the same parser for repeats, synchronization, tempo and key-on delay. */
#define FM1_GUIDE_QUEUE 8u
enum { FM1_GUIDE_OFF,FM1_GUIDE_NOTE,FM1_GUIDE_FUN };
#define FM1_GUIDE_FUN_LEAD (RETROFM_PL_CLOCK_HZ/2u)
#define FM1_GUIDE_FUN_LATE (RETROFM_PL_CLOCK_HZ*3u/20u)
typedef struct {
    retrofm_mdx_sequencer preview;
    uint64_t due[FM1_GUIDE_QUEUE],now;
    uint8_t note[FM1_GUIDE_QUEUE],head,count,selected,active,mode;
    uint32_t hits,missed;
    int error;
} fm1_guide;
void fm1_guide_start(fm1_guide *,const fm1_mdx_player *);
void fm1_guide_start_mode(fm1_guide *,const fm1_mdx_player *,unsigned mode);
void fm1_guide_stop(fm1_guide *);
void fm1_guide_step(fm1_guide *,uint64_t audible_cycles,unsigned tick_budget);
int fm1_guide_hit(fm1_guide *,unsigned midi_note);
/* Returns physical keyboard slot14..40, or41 if no playable candidate.
   Direction -1/+1 lights OCT-/OCT+ when shifting is needed. */
unsigned fm1_guide_key(const fm1_guide *,int octave,int *direction);
int fm1_guide_note(const fm1_guide *);
/* Fun Mode progress rises0..255 during the last500ms before a cue.
   Both modes light the next note key and required octave direction. */
unsigned fm1_guide_progress(const fm1_guide *);
uint64_t fm1_guide_lights(const fm1_guide *,int octave);
#endif
