#ifndef FM1_SCREEN_H
#define FM1_SCREEN_H
#include "fm1_mdx_usb.h"
#define FM1_SCREEN_BYTES 14400u
#define FM1_SCREEN_CHUNK 96u
typedef char fm1_screen_text[6][40];
typedef int (*fm1_screen_snapshot)(void *,fm1_screen_text,uint32_t *);
typedef struct {
    fm1_screen_text text;
    uint32_t token,frame,crc,last_ms;
    unsigned active;
} fm1_screen;
/* Display task owns live text; USB task owns a frozen screenshot. */
void fm1_screen_row(const fm1_screen_text,unsigned y,uint8_t rgb565[480]);
int fm1_screen_command(fm1_screen *,fm1_screen_snapshot,void (*yield)(void *),void *,const char *,uint32_t,fm1_mdx_reply,void *);
#endif
