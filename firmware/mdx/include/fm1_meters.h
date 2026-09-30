#ifndef FM1_METERS_H
#define FM1_METERS_H
#include <stdint.h>
#define FM1_FFT_SIZE 256u
#define FM1_SPECTRUM_BARS 24u
/* Task-owned tap. FFT uses complete stereo windows, never IRQ synthesis. */
typedef struct {
    int16_t samples[FM1_FFT_SIZE][2];
    unsigned count;
    uint16_t parts[16],stereo[2];
} fm1_meters;
void fm1_meters_feed(fm1_meters *,int16_t,int16_t);
void fm1_meters_peak(uint16_t *,int32_t);
/* Fixed-point, Hann-windowed, stereo-power FFT; output0..255, -60..0dB. */
void fm1_meters_spectrum(fm1_meters *,uint8_t out[FM1_SPECTRUM_BARS]);
uint8_t fm1_meters_level(uint32_t magnitude);
/* Owner-task display envelope. Levels retain 1/1000-unit precision, so
   release timing is independent of LCD frame rate. Rates are0..255 units/s. */
typedef struct {
    uint32_t level_milli,peak_milli,hold_ms;
} fm1_meter_motion;
void fm1_meter_step(fm1_meter_motion *,uint8_t input,uint32_t elapsed_ms,
                    unsigned release_per_s,unsigned hold_ms,unsigned peak_per_s);
#endif
