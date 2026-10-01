#ifndef FM1_METERS_H
#define FM1_METERS_H
#include <stdint.h>
#define FM1_FFT_SIZE 256u
#define FM1_SPECTRUM_BARS 24u
#define FM1_NOTE_BARS 32u
/* Task-owned note accumulator. Legacy FFT tap/helpers remain host utilities;
   the device uses applied note events without per-sample display work. */
typedef struct {
    int16_t samples[FM1_FFT_SIZE][2];
    unsigned count;
    uint16_t parts[16],stereo[2];
    uint32_t note_energy[FM1_NOTE_BARS];
} fm1_meters;
void fm1_meters_feed(fm1_meters *,int16_t,int16_t);
void fm1_meters_peak(uint16_t *,int32_t);
/* Fixed-point, Hann-windowed, stereo-power FFT; output0..255, -60..0dB. */
void fm1_meters_spectrum(fm1_meters *,uint8_t out[FM1_SPECTRUM_BARS]);
uint8_t fm1_meters_level(uint32_t magnitude);
void fm1_meters_note(fm1_meters *,unsigned midi,uint8_t velocity);
void fm1_meters_note_spectrum(fm1_meters *,uint8_t out[FM1_NOTE_BARS]);
/* Owner-task display envelope. Levels retain 1/1000-unit precision, so
   release timing is independent of LCD frame rate. Rates are0..65535 units/s. */
typedef struct {
    uint32_t level_milli,peak_milli,hold_ms;
} fm1_meter_motion;
void fm1_meter_step(fm1_meter_motion *,uint8_t input,uint32_t elapsed_ms,
                    unsigned release_per_s,unsigned hold_ms,unsigned peak_per_s);
/* Nominal X68000 31kHz VDISP cadence. Keep the fractional tick phase;
   rounding every tick to18ms would drift. This is not the LCD's scan rate. */
#define FM1_METER_HZ100 5545u
#define FM1_METER_PHASE_SCALE 100000u
unsigned fm1_meter_ticks(uint32_t *phase,uint32_t elapsed_ms);
typedef struct {uint32_t level_milli,tick_phase;uint8_t step,counter,off_phase;} fm1_part_motion;
/* A note retriggers an activity pulse. It decays while held; note-off is faster.
   No continuous peak input refills this envelope. */
void fm1_part_step(fm1_part_motion *,unsigned triggered,uint8_t onset,
                   uint8_t volume,unsigned held,uint32_t elapsed_ms);
typedef struct {
    uint32_t level_milli,peak_milli,tick_phase;
    uint8_t step,target,counter,peak,peak_count;
} fm1_note_motion;
/* Standard/integrating note-spectrum behavior at55.45 logical ticks/s.
   Original interrupt frequency depends on X68000 display/vector mode. */
void fm1_note_step(fm1_note_motion *,uint32_t energy,uint32_t elapsed_ms);
#endif
