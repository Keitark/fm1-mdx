/* Execute the current owner task, ring consumer and UAC target on a virtual
 * clock. SDK/peripheral boundaries are mocks; no device is opened or flashed. */
#define FM1_MDX_TARGET_HOST 1
#define FM1_USB_AUDIO 1
#include "../src/fm1_mdx_target.c"
#include <setjmp.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Replay failed line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void replay_usb_init(FILE *);
void replay_usb_stream(unsigned);
void replay_usb_tick(void);
void replay_usb_reset(void);
void replay_usb_wrap(void);
unsigned replay_usb_packets(void);
unsigned replay_usb_errors(void);
void replay_usb_note_sample(int16_t,int16_t);
volatile uint32_t fm1_display_stage;
volatile int fm1_display_error;
static unsigned ms,duration,stress,wrap_seeded,restarts,producer_stalls,song_restarts,guide_test,guide_started;
static int drift;
static unsigned lcd_us,lcd_command,lcd_y,checked_frame;
static uint8_t lcd_buffer[240][480],previous_parts[16];
static unsigned part_rises[16],part_falls[16];
static void advance(unsigned);
static uint32_t adc;
static uint64_t dac_clock;
static void (*worker)(void *),(*tick)(void *),(*alink)(void),(*keyirq)(void);
static void (*output)(void *,u8 *,int,u8);
static jmp_buf done;
void arch_spin_lock(spinlock_t *l){CHECK(!*l);*l=1;}
void arch_spin_unlock(spinlock_t *l){CHECK(*l);*l=0;}
int task_create(void (*fn)(void *),void *u,const char *name){(void)u;CHECK(!strcmp(name,"peripheral"));worker=fn;return 0;}
uint32_t timer_get_ms(void){return ms/10*10;}
void wdt_clear(void){
    if(screen_frame!=checked_frame){unsigned y;uint8_t expected[480];
        for(y=0;y<240;y++){fm1_screen_row(&screen_shown,y,expected);
#ifdef FM1_MDX_LCD_RGB444
            {unsigned x;for(x=0;x<240;x++){unsigned c=((expected[2*x]<<8)|expected[2*x+1])&0xf79e;expected[2*x]=(uint8_t)(c>>8);expected[2*x+1]=(uint8_t)c;}}
#endif
            CHECK(!memcmp(expected,lcd_buffer[y],480));}
        for(y=0;y<16;y++){part_rises[y]+=screen_shown.parts[y]>previous_parts[y];part_falls[y]+=screen_shown.parts[y]<previous_parts[y];previous_parts[y]=screen_shown.parts[y];}
        checked_frame=screen_frame;
    }
}
int clk_get(const char *s){(void)s;return 60000000;}
int iis_open(struct iis_platform_data *pd,unsigned id){CHECK(!id&&pd->data_width==8&&pd->channel_out==8&&pd->sr_points==128);return 0;}
int iis_set_sample_rate(unsigned rate,unsigned id){CHECK(!id&&rate==44100);return 0;}
void iis_set_dec_data_handler(unsigned id,void (*fn)(void *,u8 *,int,u8),void *u){CHECK(!id&&!u);output=fn;}
void iis_channel_on(unsigned ch,unsigned id){CHECK(ch==8&&!id);}
void iis_channel_off(unsigned ch,unsigned id){CHECK(ch==8&&!id);}
void iis_close(unsigned id){CHECK(!id);}
void iis_irq_handler(unsigned id){
    int32_t b[128];unsigned i,n=wr-rd;CHECK(!id&&output);
    if(stress&&!wrap_seeded){rd+=0xfffff800u;wr+=0xfffff800u;replay_usb_wrap();wrap_seeded=1;}
    if(n>64)n=64;for(i=0;i<n;i++)replay_usb_note_sample(ring[(rd+i)&2047][0],ring[(rd+i)&2047][1]);
    output(0,(u8 *)b,512,3);
}
void request_irq(unsigned id,unsigned priority,void (*fn)(void),unsigned cpu){(void)priority;CHECK(!cpu);if(id==IRQ_ALNK_IDX)alink=fn;else if(id==IRQ_SPI2_IDX)keyirq=fn;else CHECK(0);}
void unrequest_irq(unsigned id,unsigned cpu){CHECK(!cpu);if(id==IRQ_ALNK_IDX)alink=0;else if(id==IRQ_SPI2_IDX)keyirq=0;}
void bit_clr_ie(unsigned id,unsigned cpu){(void)id;CHECK(!cpu);}
int sys_usec_timer_add(void *u,void (*fn)(void *),uint32_t period,unsigned char unit,unsigned char cpu){CHECK(!u&&period==1000&&unit==1&&!cpu);tick=fn;return 1;}
void sys_usec_timer_del(int id){CHECK(id==1);tick=0;}
int fm1_display_test_init(void){return 0;}
int fm1_display_mdx_stream_start(void){return 0;}
int fm1_display_test_frame(uint32_t n){(void)n;return 0;}
void fm1_display_test_stop(void){}
int fm1_display_write(int data,const uint8_t *b,size_t n){CHECK(b&&n&&(data==0||data==1));
    if(!data){CHECK(n==1);lcd_command=b[0];}
    else if(lcd_command==0x2b){CHECK(n==4);lcd_y=(b[0]<<8)|b[1];CHECK(lcd_y==((b[2]<<8)|b[3]));}
    else if(n==MDX_ROW_BYTES){CHECK(lcd_command==0x2c && lcd_y<240);
#ifdef FM1_MDX_LCD_RGB444
        {unsigned x;for(x=0;x<240;x++){unsigned k=x/2*3,r,g,blue,c;
            if(x&1){r=b[k+1]&15;g=b[k+2]>>4;blue=b[k+2]&15;}
            else {r=b[k]>>4;g=b[k]&15;blue=b[k+1]>>4;}
            c=(r<<12)|(g<<7)|(blue<<1);lcd_buffer[lcd_y][2*x]=(uint8_t)(c>>8);lcd_buffer[lcd_y][2*x+1]=(uint8_t)c;}}
#else
        memcpy(lcd_buffer[lcd_y],b,480);
#endif
#ifdef FM1_MDX_LCD_BAUD
        lcd_us+=400*MDX_ROW_BYTES*12/(480*(60/(FM1_MDX_LCD_BAUD+1)));
#else
        lcd_us+=400*MDX_ROW_BYTES/480;
#endif
        while(lcd_us>=1000){lcd_us-=1000;advance(1);}}return 0;}
int fm1_wl82_keyscan_async_start(fm1_wl82_keyscan *s,void *u,uint32_t (*clock)(void *)){CHECK(!u&&clock);s->running=1;return 0;}
void fm1_wl82_keyscan_async_step(fm1_wl82_keyscan *s){(void)s;}
void fm1_wl82_keyscan_async_kick(fm1_wl82_keyscan *s){s->sequence++;}
int fm1_wl82_keyscan_async_raw(fm1_wl82_keyscan *s,uint8_t rows[11]){if(s->sequence==s->consumed)return FM1_NES_BUSY;s->consumed=s->sequence;memset(rows,0x3f,11);return 0;}
void fm1_wl82_keyscan_stop(fm1_wl82_keyscan *s){s->running=0;}
void fm1_wl82_keyscan_lights(fm1_wl82_keyscan *s,uint64_t slots){(void)s;(void)slots;}
uint32_t fm1_volume_test_read(uint32_t a){if(a==0x13100)return adc|0x80;if(a==0x13104)return 256;return 0;}
void fm1_volume_test_write(uint32_t a,uint32_t v){if(a==0x13100)adc=v&~0xc0u;}
static void advance(unsigned n){
    while(n--){
        ms++;
        if(guide_test && !guide_started && ms>=50 && uploaded_song && !control.request){control.request=FM1_MDX_GUIDE;control.a=guide_test;guide_started=1;}
        if(duration>=600&&ms%360000==0){CHECK(!control.request);control.request=FM1_MDX_PLAY;song_restarts++;}
        if(stress&&ms%15000==0){replay_usb_stream(0);restarts++;}
        if(stress&&ms%15000==5&&ms>15000)replay_usb_stream(1);
        if(stress&&ms%61000==0){replay_usb_reset();replay_usb_stream(1);restarts++;}
        if(tick)tick(0);if(keyirq)keyirq();
        /*64 DAC frames per callback,44.1kHz clock with optional ppm drift. */
        dac_clock+=(uint64_t)44100*(unsigned)(1000000+drift);
        while(dac_clock>=UINT64_C(64000000000)){dac_clock-=UINT64_C(64000000000);if(alink)alink();}
        replay_usb_tick();
        if(ms>=duration*1000u)longjmp(done,1);
    }
}
void os_time_dly(int n){
    CHECK(n==1);advance(10); /* pinned SDK OS tick =10ms */
    if(stress&&ms/47000>producer_stalls){producer_stalls++;advance(60);}
}
static unsigned char *read_file(const char *name,size_t *n){
    FILE *f=fopen(name,"rb");long size;unsigned char *b;
    CHECK(f);CHECK(!fseek(f,0,SEEK_END));size=ftell(f);rewind(f);CHECK(size>0&&size<=192*1024);
    b=malloc((size_t)size);CHECK(b);CHECK(fread(b,1,(size_t)size,f)==(size_t)size);fclose(f);*n=(size_t)size;return b;
}
static void word(FILE *f,unsigned v,unsigned n){while(n--){fputc(v&255,f);v>>=8;}}
static void header(FILE *f,unsigned bytes){
    rewind(f);fwrite("RIFF",1,4,f);word(f,36+bytes,4);fwrite("WAVEfmt ",1,8,f);word(f,16,4);word(f,1,2);word(f,2,2);word(f,48000,4);word(f,192000,4);word(f,4,2);word(f,16,2);fwrite("data",1,4,f);word(f,bytes,4);
}
int main(int argc,char **argv){
    size_t mn,pn;unsigned char *m=0,*p=0;FILE *f;char usb[200];unsigned errors;
    CHECK(argc==7 || argc==8);duration=(unsigned)atoi(argv[4]);drift=atoi(argv[5]);stress=!strcmp(argv[6],"stress");
    CHECK(duration>=1&&duration<=3600&&drift>=-1000&&drift<=1000);
    CHECK(stress||!strcmp(argv[6],"normal")||!strcmp(argv[6],"guide")||!strcmp(argv[6],"timing"));
    if(!strcmp(argv[6],"guide"))guide_test=1;
    if(!strcmp(argv[6],"timing"))guide_test=2;
    if(!strcmp(argv[1],"--demo")){mn=fm1_demo_mdx_size;pn=fm1_demo_pdx_size;m=(unsigned char *)fm1_demo_mdx;p=(unsigned char *)fm1_demo_pdx;}
    else {m=read_file(argv[1],&mn);p=read_file(argv[2],&pn);}
    CHECK(mn+pn+12<=sizeof(upload.bytes));memcpy(upload.bytes+12,m,mn);memcpy(upload.bytes+12+mn,p,pn);
    upload.mdx_size=(uint32_t)mn;upload.pdx_size=(uint32_t)pn;upload.ready=1;control.request=FM1_MDX_PLAY;
    f=fopen(argv[3],"wb");CHECK(f);header(f,0);replay_usb_init(f);
    CHECK(!fm1_peripheral_start_task());CHECK(worker);if(!setjmp(done))worker(0);
    fm1_usb_audio_status(usb,sizeof(usb));errors=replay_usb_errors();
    header(f,replay_usb_packets()*192u);CHECK(!fclose(f));
    printf("{\"virtual_seconds\":%u,\"ppm\":%d,\"stress\":%u,\"packets\":%u,\"dac_frames\":%u,\"synth_missing\":%u,\"rebuffer_events\":%u,\"usb_errors\":%u,\"stream_restarts\":%u,\"producer_stalls\":%u,\"wrap_seeded\":%u,\"player_error\":%d,\"lcd_error\":%d}\n",duration,drift,stress,replay_usb_packets(),frames,underruns,rebuffer_events,errors,restarts,producer_stalls,wrap_seeded,player.error,lcd_error);
    fprintf(stderr,"Song restarts=%u, playing=%u, sequencer ended=%u\n",song_restarts,player.playing,player.sequence.ended);
    fprintf(stderr,"%s",usb);
    fprintf(stderr,"Display: frames=%u fps=%.2f rows=%u skipped=%u max_ms=%u meter_ticks=%u (scaled400us/565-row wire-cost model)\n",screen_frame,(double)screen_frame/duration,ui_rows,ui_skipped,ui_max_ms,ui_ticks);
    {unsigned part;fprintf(stderr,"Part motion rises/falls:");for(part=0;part<16;part++)fprintf(stderr," %u:%u/%u",part+1,part_rises[part],part_falls[part]);fputc('\n',stderr);}
    if(duration>=5)CHECK(screen_frame>duration*10 && ui_skipped>ui_rows);
    if(strcmp(argv[1],"--demo")){free(m);free(p);}
    CHECK(!player.error&&!lcd_error&&!audio_error);if(!stress)CHECK(!underruns&&!errors);
    if(stress&&duration>=65)CHECK(wrap_seeded&&restarts&&producer_stalls&&rebuffer_events);
    if(guide_test){CHECK(guide_started && guide.active && guide.mode==guide_test && !guide.error && guide.missed>0);fprintf(stderr,"Guide: mode=%u missed=%u next=%d error=%d\n",guide.mode,guide.missed,fm1_guide_note(&guide),guide.error);}
    if(argc==8){unsigned y,x;uint8_t row[240];FILE *shot=fopen(argv[7],"wb");CHECK(shot);
        for(y=0;y<240;y++){fm1_screen_indices(&screen_shown,y,row);for(x=0;x<240;x+=2)fputc((row[x]<<4)|row[x+1],shot);}CHECK(!fclose(shot));}
    return 0;
}
