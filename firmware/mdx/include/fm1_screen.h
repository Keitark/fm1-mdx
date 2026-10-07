#ifndef FM1_SCREEN_H
#define FM1_SCREEN_H
#include "fm1_mdx_usb.h"
#define FM1_SCREEN_BYTES 28800u
#define FM1_SCREEN_CHUNK 96u
/* LCD request cadence is separate from MMDSP's55.45Hz envelope clock. */
#define FM1_SCREEN_HZ100 3000u
/* Compact frozen state, not a115KiB framebuffer. */
typedef struct {
    char title[33],subtitle[39],credit[128];
    uint8_t spectrum[32],spectrum_hold[32],parts[16],hold[16],stereo[2];
    uint16_t credit_scroll;
    uint16_t mutes;
    uint8_t selected,running,tracks,uploaded;
    int8_t octave;
    uint8_t guide; /*0 off,1 note,2 fun */
    uint8_t guide_pending,guide_progress;
    int8_t guide_note,guide_direction;
    uint32_t seconds;
} fm1_screen_view;
typedef int (*fm1_screen_snapshot)(void *,fm1_screen_view *,uint32_t *);
typedef struct {
    fm1_screen_view view;
    uint32_t token,frame,crc,last_ms;
    unsigned active;
} fm1_screen;
extern const uint16_t fm1_screen_palette[16];
void fm1_screen_indices(const fm1_screen_view *,unsigned y,uint8_t indices[240]);
void fm1_screen_row(const fm1_screen_view *,unsigned y,uint8_t rgb565[480]);
void fm1_screen_row444(const fm1_screen_view *,unsigned y,uint8_t rgb444[360]);
int fm1_screen_row_changed(const fm1_screen_view *,const fm1_screen_view *,unsigned y);
/* Precompute a frozen view's dirty rows, quantizing each meter just once. */
void fm1_screen_dirty_rows(const fm1_screen_view *,const fm1_screen_view *,uint8_t rows[240]);
void fm1_screen_title(fm1_screen_view *,const uint8_t *,size_t);
int fm1_screen_command(fm1_screen *,fm1_screen_snapshot,void (*yield)(void *),void *,const char *,uint32_t,fm1_mdx_reply,void *);
#endif
