#ifndef FM1_USB_AUDIO_TARGET_H
#define FM1_USB_AUDIO_TARGET_H
#include <stdint.h>
#include <stddef.h>
void fm1_usb_audio_init(void);
void fm1_usb_audio_stop(void);
void fm1_usb_audio_dac(int32_t *,unsigned);
void fm1_usb_audio_status(char *,size_t);
void fm1_usb_audio_transport_status(char *,size_t);
#endif
