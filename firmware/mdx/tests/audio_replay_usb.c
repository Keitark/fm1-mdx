/* Host boundary for the real UAC target, including its packet encoder.
 * The fake SDK copies complete packets; it does not emulate WL82 USB DMA. */
#define FM1_UAC_TARGET_HOST 1
#include "../../usb-audio/target.c"
#include <stdlib.h>
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"USB replay boundary failed: %s\n",#x);exit(1);}}while(0)
static struct usb_device_t device={USB_CONFIGURED};
static u8 *dma[2];
static void (*interrupts[2])(struct usb_device_t *,u32);
static u32 (*handlers[5])(struct usb_device_t *,struct usb_ctrlrequest *);
static void (*resets[5])(struct usb_device_t *,u32);
static FILE *capture;
static unsigned packets;
static unsigned source_peak;
void replay_usb_note_sample(int16_t l,int16_t r){
    unsigned a=(unsigned)abs((int)l),b=(unsigned)abs((int)r);
    if(a>source_peak)source_peak=a;if(b>source_peak)source_peak=b;
}
usb_dev usb_device2id(const struct usb_device_t *d){REQUIRE(d==&device);return 0;}
struct usb_device_t *usb_id2device(usb_dev id){REQUIRE(!id);return &device;}
u32 usb_g_iso_read(usb_dev id,u32 ep,void *p,u32 n,u32 last){REQUIRE(!id&&ep==1&&!p&&n==192&&!last);return 0;}
unsigned fm1_usb_packet_write(unsigned id,unsigned ep,const uint8_t *p,unsigned n,const volatile unsigned *epoch,unsigned expected){
    unsigned i;
    REQUIRE(!id&&ep==1&&p&&n==192&&dma[1]&&*epoch==expected);
    memcpy(dma[1],p,n);
    /* Linear capture conversion cannot exceed the source's magnitude.
     * This checks the encoded bytes independently of the error counters. */
    for(i=0;i<n;i+=2){int value=(int16_t)(dma[1][i]|((unsigned)dma[1][i+1]<<8));REQUIRE((unsigned)abs(value)<=source_peak);}
    REQUIRE(fwrite(dma[1],1,n,capture)==n);packets++;return n;
}
void usb_clr_intr_txe(usb_dev id,u32 ep){REQUIRE(!id&&ep==1);}
void usb_clr_intr_rxe(usb_dev id,u32 ep){REQUIRE(!id&&ep==1);}
void usb_enable_ep(usb_dev id,u32 ep){REQUIRE(!id&&ep==1);}
u32 usb_read_txcsr(usb_dev id,u32 ep){REQUIRE(!id&&ep==1);return 0;}
void usb_write_txcsr(usb_dev id,u32 ep,u32 v){REQUIRE(!id&&ep==1&&v==(TXCSRP_FlushFIFO|TXCSRP_ClrDataTog|TXCSRP_ISOCHRONOUS));}
void usb_write_rxcsr(usb_dev id,u32 ep,u32 v){REQUIRE(!id&&ep==1&&v==(RXCSRP_FlushFIFO|RXCSRP_ClrDataTog|RXCSRP_ISOCHRONOUS));}
void usb_set_intr_txe(usb_dev id,u32 ep){REQUIRE(!id&&ep==1&&interrupts[1]);}
void usb_set_intr_rxe(usb_dev id,u32 ep){REQUIRE(!id&&ep==1&&interrupts[0]);}
u32 usb_g_ep_config(usb_dev id,u32 ep,u32 type,u32 ie,u8 *p,u32 n){
    REQUIRE(!id&&type==1&&!ie&&n==192&&!((uintptr_t)p%64));dma[ep>>7]=p;return 0;
}
u32 usb_g_set_intr_hander(usb_dev id,u32 ep,void (*h)(struct usb_device_t *,u32)){
    REQUIRE(!id&&(ep==1||ep==0x81));interrupts[ep>>7]=h;return 0;
}
u32 usb_set_interface_hander(usb_dev id,u32 i,u32 (*h)(struct usb_device_t *,struct usb_ctrlrequest *)){
    REQUIRE(!id&&i>=2&&i<5);handlers[i]=h;return i;
}
u32 usb_set_reset_hander(usb_dev id,u32 i,void (*h)(struct usb_device_t *,u32)){
    REQUIRE(!id&&i>=2&&i<5);resets[i]=h;return i;
}
void usb_set_setup_phase(struct usb_device_t *d,u8 p){REQUIRE(d==&device&&p==0);}
void *usb_get_setup_buffer(const struct usb_device_t *d){REQUIRE(d==&device);return device.setup;}
u8 *usb_set_data_payload(struct usb_device_t *d,struct usb_ctrlrequest *r,const void *p,u32 n){
    REQUIRE(d==&device&&p==device.setup&&n==r->wLength&&n<=2);return device.setup;
}
void replay_usb_stream(unsigned on){struct usb_ctrlrequest r={1,11,(uint16_t)on,4,0};handlers[4](&device,&r);}
void replay_usb_init(FILE *f){u8 descriptor[173];u32 itf=2;capture=f;fm1_usb_audio_init();REQUIRE(fm1_uac_desc_config(0,descriptor,&itf)==173);replay_usb_stream(1);}
void replay_usb_tick(void){if(interrupts[1])interrupts[1](&device,1);}
void replay_usb_reset(void){resets[2](&device,2);}
void replay_usb_wrap(void){uint32_t offset=0xfffffc00u;bridge.capture.rd+=offset;bridge.capture.wr+=offset;}
unsigned replay_usb_packets(void){return packets;}
unsigned replay_usb_errors(void){return bridge.capture.underruns+bridge.capture.overruns+bridge.bad_packets;}
