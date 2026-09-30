#ifdef FM1_UAC_TARGET_HOST
#include "target_fake.h"
#else
#include "app_config.h"
#include "system/includes.h"
#include "system/spinlock.h"
#include "usb/device/usb_stack.h"
#endif
#include "bridge.h"
#include "target.h"
#include <stdio.h>
#include <string.h>
static fm1_uac_bridge bridge;
static spinlock_t lock;
static unsigned armed;
#ifdef _MSC_VER
static __declspec(align(64)) u8 fm1_uac_dma[2][256];
#else
static u8 fm1_uac_dma[2][256] __attribute__((aligned(64)));
#endif
static unsigned take(void){unsigned f;local_irq_save(f);arch_spin_lock(&lock);return f;}
static void release(unsigned f){arch_spin_unlock(&lock);local_irq_restore(f);}
static void rx(struct usb_device_t *d,u32 ep) {
    unsigned f,n;const usb_dev id=usb_device2id(d);(void)ep;
    n=usb_g_iso_read(id,1,NULL,192,0);
    f=take();if(armed)fm1_uac_receive(&bridge,fm1_uac_dma[0],n);release(f);
}
static void tx(struct usb_device_t *d,u32 ep) {
    unsigned f,n;const usb_dev id=usb_device2id(d);(void)ep;
    f=take();n=armed?fm1_uac_transmit(&bridge,fm1_uac_dma[1],192):0;release(f);
    if(n)usb_g_iso_write(id,1,NULL,n);
}
static void stream(struct usb_device_t *d,unsigned direction,int on) {
    unsigned f=take();const usb_dev id=usb_device2id(d);
    if(!armed)on=0;
    fm1_uac_stream(&bridge,direction,on);release(f);
    if(direction){usb_clr_intr_txe(id,1);usb_g_set_intr_hander(id,0x81,NULL);}
    else {usb_clr_intr_rxe(id,1);usb_g_set_intr_hander(id,1,NULL);}
    if(on) {
        if(direction)usb_enable_ep(id,1);
        usb_g_set_intr_hander(id,direction?0x81:1,direction?tx:rx);
        usb_g_ep_config(id,direction?0x81:1,USB_ENDPOINT_XFER_ISOC,1,fm1_uac_dma[direction],192);
        if(direction)tx(d,1);
    }
}
static void reset(struct usb_device_t *d,u32 itf){(void)itf;stream(d,0,0);stream(d,1,0);}
static u32 setup(struct usb_device_t *d,struct usb_ctrlrequest *r) {
    unsigned f;u8 *reply=usb_get_setup_buffer(d);reply[0]=reply[1]=0;
    if(d->bDeviceStates!=USB_CONFIGURED || !fm1_uac_interface_request(r->bRequestType,r->bRequest,r->wValue,r->wIndex,r->wLength)) {
        usb_set_setup_phase(d,USB_EP0_SET_STALL);return 0;
    }
    if(r->bRequest==11) {
        if(r->wIndex>2)stream(d,r->wIndex==4,r->wValue);
        usb_set_setup_phase(d,USB_EP0_STAGE_SETUP);
    } else {
        if(r->bRequest==10 && r->wIndex>2){f=take();reply[0]=r->wIndex==4?bridge.in_active:bridge.out_active;release(f);}
        usb_set_data_payload(d,r,reply,r->wLength);
    }
    return 0;
}
u32 fm1_uac_desc_config(usb_dev id,u8 *out,u32 *itf) {
    unsigned i;if(id!=FM1_USB_CONTROLLER || *itf!=2)return 0;
    for(i=2;i<5;i++) {
        if(usb_set_interface_hander(id,i,setup)!=i || usb_set_reset_hander(id,i,reset)!=i)return 0;
    }
    memcpy(out,fm1_uac_descriptor,sizeof(fm1_uac_descriptor));*itf=5;return sizeof(fm1_uac_descriptor);
}
void fm1_usb_audio_init(void){unsigned f=take();armed=1;release(f);}
void fm1_usb_audio_stop(void) {
    unsigned f=take();armed=0;release(f);reset(usb_id2device(FM1_USB_CONTROLLER),2);
}
void fm1_usb_audio_dac(int32_t *p,unsigned n){unsigned f=take();if(armed)fm1_uac_dac(&bridge,p,n);release(f);}
void fm1_usb_audio_status(char *out,size_t n) {
    unsigned f=take();snprintf(out,n,"MDX AUDIO rate=48000 bits=16 channels=2 out=%u in=%u rx=%u tx=%u bad=%u play_fill=%u capture_fill=%u under=%u,%u over=%u,%u\n",
        bridge.out_active,bridge.in_active,bridge.rx_packets,bridge.tx_packets,bridge.bad_packets,bridge.playback.wr-bridge.playback.rd,bridge.capture.wr-bridge.capture.rd,
        bridge.playback.underruns,bridge.capture.underruns,bridge.playback.overruns,bridge.capture.overruns);release(f);
}
