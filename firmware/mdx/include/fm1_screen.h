#ifndef FM1_SCREEN_H
#define FM1_SCREEN_H
#include "fm1_mdx_usb.h"
#define FM1_SCREEN_BYTES 28800u
#define FM1_SCREEN_CHUNK 96u
/* Compact frozen state, not a115KiB framebuffer. */
typedef struct {
    char title[33],subtitle[39];
    uint8_t spectrum[24],parts[16],hold[16],stereo[2];
    uint16_t mutes;
    uint8_t selected,running,tracks,uploaded;
    int8_t octave;
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
void fm1_screen_title(fm1_screen_view *,const uint8_t *,size_t);
int fm1_screen_command(fm1_screen *,fm1_screen_snapshot,void (*yield)(void *),void *,const char *,uint32_t,fm1_mdx_reply,void *);
#endif
