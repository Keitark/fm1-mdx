#define FM1_MDX_TARGET_HOST 1
#include "../src/fm1_mdx_target.c"
#include <setjmp.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
volatile uint32_t fm1_display_stage;
volatile int fm1_display_error;
static unsigned ms,opened,closed,timer_deleted,key_stopped,lcd_stopped;
static uint32_t adc;
static unsigned lcd_command,lcd_y,lcd_first,lcd_last;
static void (*worker)(void *),(*tick)(void *),(*alink)(void),(*keyirq)(void);
static void (*output)(void *,u8 *,int,u8);
static jmp_buf done;
void arch_spin_lock(spinlock_t *l){CHECK(!*l);*l=1;}
void arch_spin_unlock(spinlock_t *l){CHECK(*l);*l=0;}
int task_create(void (*fn)(void *),void *u,const char *name){(void)u;CHECK(!strcmp(name,"peripheral"));worker=fn;return 0;}
uint32_t timer_get_ms(void){return ms;}
void wdt_clear(void){}
int clk_get(const char *s){(void)s;return 60000000;}
int iis_open(struct iis_platform_data *pd,unsigned id){CHECK(!id && pd->data_width==8 && pd->channel_out==8 && pd->sr_points==128);opened++;return 0;}
int iis_set_sample_rate(unsigned rate,unsigned id){CHECK(!id && rate==44100);return 0;}
void iis_set_dec_data_handler(unsigned id,void (*fn)(void *,u8 *,int,u8),void *u){CHECK(!id && !u);output=fn;}
void iis_channel_on(unsigned ch,unsigned id){CHECK(ch==8 && !id);}
void iis_channel_off(unsigned ch,unsigned id){CHECK(ch==8 && !id);}
void iis_close(unsigned id){CHECK(!id);closed++;}
void iis_irq_handler(unsigned id){int32_t b[128];CHECK(!id && output);output(0,(u8 *)b,512,3);}
void request_irq(unsigned id,unsigned priority,void (*fn)(void),unsigned cpu){(void)priority;CHECK(!cpu);if(id==IRQ_ALNK_IDX)alink=fn;else if(id==IRQ_SPI2_IDX)keyirq=fn;else CHECK(0);}
void unrequest_irq(unsigned id,unsigned cpu){CHECK(!cpu);if(id==IRQ_ALNK_IDX)alink=0;else if(id==IRQ_SPI2_IDX)keyirq=0;}
void bit_clr_ie(unsigned id,unsigned cpu){(void)id;CHECK(!cpu);}
int sys_usec_timer_add(void *u,void (*fn)(void *),uint32_t period,unsigned char unit,unsigned char cpu){CHECK(!u && period==1000 && unit==1 && !cpu);tick=fn;return 1;}
void sys_usec_timer_del(int id){CHECK(id==1);tick=0;timer_deleted++;}
int fm1_display_test_init(void){return 0;}
int fm1_display_mdx_stream_start(void){return 0;}
int fm1_display_test_frame(uint32_t n){(void)n;return 0;}
void fm1_display_test_stop(void){lcd_stopped++;}
int fm1_display_write(int data,const uint8_t *b,size_t n){CHECK(b && n && (data==0||data==1));
    if(!data){CHECK(n==1);lcd_command=b[0];}
    else if(lcd_command==0x2b){CHECK(n==4 && !b[0] && !b[2] && b[1]==b[3] && b[1]<240);lcd_y=b[1];}
    else if(n==480){CHECK(lcd_command==0x2c);if(!lcd_y)lcd_first++;if(lcd_y==239)lcd_last++;}
    return 0;}
int fm1_wl82_keyscan_async_start(fm1_wl82_keyscan *s,void *u,uint32_t (*clock)(void *)){CHECK(!u && clock);s->running=1;return 0;}
void fm1_wl82_keyscan_async_step(fm1_wl82_keyscan *s){(void)s;}
void fm1_wl82_keyscan_async_kick(fm1_wl82_keyscan *s){s->sequence++;}
int fm1_wl82_keyscan_async_raw(fm1_wl82_keyscan *s,uint8_t rows[11]){if(ms>=70)return FM1_NES_IO_ERROR;if(s->sequence==s->consumed)return FM1_NES_BUSY;s->consumed=s->sequence;memset(rows,0x3f,11);return 0;}
void fm1_wl82_keyscan_stop(fm1_wl82_keyscan *s){s->running=0;key_stopped++;}
void fm1_wl82_keyscan_lights(fm1_wl82_keyscan *s,uint64_t slots){(void)s;(void)slots;}
uint32_t fm1_volume_test_read(uint32_t a){if(a==0x13100)return adc|0x80;if(a==0x13104)return 256;return 0;}
void fm1_volume_test_write(uint32_t a,uint32_t v){if(a==0x13100)adc=v&~0xc0u;}
static char answer[200];
static void reply(void *u,const char *s){(void)u;snprintf(answer,sizeof(answer),"%s",s);}
static void command(const char *s){CHECK(fm1_mdx_usb_command(s,ms,reply,0));CHECK(strncmp(answer,"ERR",3));}
void os_time_dly(int n){
    unsigned i;
    if(control.quiescent){CHECK(n==100);longjmp(done,1);}
    CHECK(n==1);
    for(i=0;i<(unsigned)n;i++) {
        ms++;if(tick)tick(0);if(keyirq)keyirq();if(alink)alink();
        if(ms==20){CHECK(player.mute_mask==0x100);command("MDX MUTE 00 1");}
        if(ms==21)command("MDX GUIDE 1");
        if(ms==22)command("MDX NOTE 3c 1");
        if(ms==35){CHECK(player.live_note[0]==60);CHECK(player.mute_mask&1);CHECK(guide_enabled && guide.active);command("MDX GUIDE");CHECK(strstr(answer,"enabled=1"));}
        if(ms==40)command("MDX STOP");
        if(ms==45){CHECK(!control.running && !control.request);command("MDX BEGIN 00000010 00000000");CHECK(upload.active);}
        if(ms==46){command("MDX ABORT");CHECK(!upload.active);command("MDX DEMO");}
        if(ms==55)encoders.count[0]++;
        if(ms==56){CHECK(player.selected==0);encoders.count[0]++;}
        if(ms==65){CHECK(player.selected==1 && player.mute_mask==0x102);CHECK(guide.active && guide.selected==1);command("MDX INPUT");CHECK(strstr(answer,"enc=2,0,0,0,0,0,0"));}
        if(ms==80){CHECK(control.running && !player.error);CHECK(mdx_volume.valid && mdx_volume.running && mdx_volume.target==32);CHECK(key_error==FM1_NES_IO_ERROR && !scan_enabled);command("MDX VOLUME");CHECK(strstr(answer,"target=32") && strstr(answer,"running=1"));fm1_peripheral_session_cancel();CHECK(control.running);}
        if(ms==100)fm1_peripheral_cancel();
        CHECK(ms<200);
    }
}
static void panel_tests(void) {
    unsigned i;
    CHECK(!fm1_mdx_load(&player,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    control.available=control.running=1;
    panel_edges(UINT64_C(1)<<3,0,UINT64_C(1)<<3);CHECK(control.request==FM1_MDX_GUIDE && control.a==1);control.request=0; /* SEL */
    panel_edges(UINT64_C(1)<<12,0,UINT64_C(1)<<12);CHECK(control.request==FM1_MDX_STOP);control.request=0;
    control.running=0;panel_edges(UINT64_C(1)<<12,0,0);CHECK(control.request==FM1_MDX_DEMO);control.request=0;control.running=1;
    panel_edges(4,0,4);CHECK(control.request==FM1_MDX_MUTE);control.request=0;
    CHECK(!fm1_mdx_mute(&player,0,1));
    panel_edges(UINT64_C(1)<<14,0,UINT64_C(1)<<14);CHECK(player.live_note[0]==53);
    panel_edges(2,0,(UINT64_C(1)<<14)|2);CHECK(keyboard_octave==1 && player.live_note[0]==53 && !control.request);
    panel_edges(0,UINT64_C(1)<<14,0);CHECK(player.live_note[0]==-1); /* old pitch released */
    panel_edges(UINT64_C(1)<<14,0,UINT64_C(1)<<14);CHECK(player.live_note[0]==65);
    panel_edges(UINT64_C(1)<<15,0,(UINT64_C(1)<<14)|(UINT64_C(1)<<15));CHECK(player.live_note[0]==66);
    panel_edges(0,UINT64_C(1)<<14,UINT64_C(1)<<15);CHECK(player.live_note[0]==66);
    panel_edges(0,UINT64_C(1)<<15,0);CHECK(player.live_note[0]==-1);
    for(i=0;i<10;i++)panel_edges(1,0,1);
    CHECK(keyboard_octave==-3 && player.selected==0 && !control.request);
    panel_edges(UINT64_C(1)<<14,0,0);CHECK(player.live_note[0]==17);
    for(i=0;i<10;i++)panel_edges(2,0,2);
    CHECK(keyboard_octave==2);
    panel_edges(UINT64_C(1)<<40,0,0);CHECK(player.live_note[0]==103);
    panel_edges(3,0,3);CHECK(keyboard_octave==2); /* opposite buttons cancel */
    silence();CHECK(keyboard_slot==41 && player.live_note[0]==-1);
    action(FM1_MDX_DEMO,0,0);CHECK(player.mute_mask==0x100 && !uploaded_song);
    memcpy(upload.bytes+12,fm1_demo_mdx,fm1_demo_mdx_size);
    memcpy(upload.bytes+12+fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size);
    upload.mdx_size=(uint32_t)fm1_demo_mdx_size;upload.pdx_size=(uint32_t)fm1_demo_pdx_size;
    action(FM1_MDX_PLAY,0,0);CHECK(!player.mute_mask && uploaded_song);
    action(FM1_MDX_GUIDE,1,0);CHECK(guide_enabled && (player.mute_mask&(1u<<player.selected)));
    guide_update(2048,2);CHECK(guide.active);
    action(FM1_MDX_SELECT,1,0);CHECK(player.selected==1 && player.mute_mask==2);
    guide_update(2048,2);CHECK(guide.active && guide.selected==1 && !guide.error);
    player.sequence.tracks[0].opm_volume=0;
    control.running=player.playing;fm1_mdx_song_write(&player,8,0x78);ui_update(33);
    CHECK(ui_live.selected==1 && ui_live.mutes==2 && ui_live.parts[0]==245);
    CHECK(!fm1_mdx_note(&player,65,1));action(FM1_MDX_SELECT,1,0);
    CHECK(player.live_note[1]==65 && player.mute_mask==2);
    action(FM1_MDX_SELECT,0,0);CHECK(player.mute_mask==1 && player.live_note[1]==-1);
    guide_update(2048,2);CHECK(guide.active && guide.selected==0);
    action(FM1_MDX_GUIDE,0,0);CHECK(!guide_enabled && !guide.active);
    silence();keyboard_octave=0;memset(&control,0,sizeof(control));
}
static void audio_boundary_tests(void) {
    unsigned i;int32_t out[128];char diagnostics[200];
    fm1_audio_startup_reset(&envelope);envelope.frames_left=0;envelope.gain_q7=64;
    mdx_volume.valid=1;mdx_volume.target=64;audio_playing=primed=1;rd=0;wr=64;
    for(i=0;i<64;i++){ring[i][0]=1000;ring[i][1]=-1000;}
    audio_output(0,(u8 *)out,512,3);
    CHECK(rd==wr && primed && !underruns && !rebuffer_events);
    for(i=64;i<128;i++){ring[i][0]=2000;ring[i][1]=-2000;}wr=128;ms=2;
    audio_output(0,(u8 *)out,512,3);
    CHECK(primed && rd==wr && out[0]==2000*128 && out[1]==-2000*128 && !underruns);
    ms=4;audio_output(0,(u8 *)out,512,3);
    CHECK(!primed && underruns==64 && rebuffer_events==1 && rebuffer_frames==64);
    ms=6;audio_output(0,(u8 *)out,512,3);CHECK(rebuffer_frames==128 && underruns==64);
    for(i=0;i<1470;i++){ring[(wr+i)&2047][0]=3000;ring[(wr+i)&2047][1]=-3000;}wr+=1470;
    ms=8;audio_output(0,(u8 *)out,512,3);CHECK(primed && out[0]==3000*128);
    ms=18;audio_output(0,(u8 *)out,512,3);CHECK(!late_callbacks && callback_gap_ms==10);
    ms=38;audio_output(0,(u8 *)out,512,3);CHECK(late_callbacks==1 && callback_gap_ms==20);
    command("MDX TIMING");snprintf(diagnostics,sizeof(diagnostics),"%s",answer);
    CHECK(strstr(diagnostics,"late=1") && strstr(diagnostics,"rebuffer=1"));
    silence();ms=40;audio_output(0,(u8 *)out,512,3);CHECK(rebuffer_frames==128);
    rd=wr=underruns=frames=callback_ms=callback_gap_ms=late_callbacks=rebuffer_events=rebuffer_frames=render_max_ms=0;
    callback_seen=0;queue_min=2048;ms=0;memset(&mdx_volume,0,sizeof(mdx_volume));
}
static void timing_seed(unsigned first,unsigned second) {
    memset(&guide,0,sizeof(guide));guide.active=1;guide.mode=FM1_GUIDE_TIMING;
    guide.selected=player.selected;guide.count=2;guide.note[0]=(uint8_t)first;guide.note[1]=(uint8_t)second;
    guide.due[0]=RETROFM_PL_CLOCK_HZ*2u;guide.due[1]=RETROFM_PL_CLOCK_HZ*3u;
}
static void timing_panel_tests(void) {
    static fm1_mdx_player reference;
    unsigned slot;uint8_t patch[192];int16_t actual[256],expected[256];
    CHECK(!fm1_mdx_load(&player,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    CHECK(!fm1_mdx_load(&reference,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    CHECK(!fm1_mdx_mute(&reference,0,1));fm1_mdx_song_write(&reference,0x40,0x32);
    control.available=control.running=1;action(FM1_MDX_GUIDE,FM1_GUIDE_TIMING,0);
    CHECK(guide_enabled==2 && player.mute_mask==1);
    fm1_mdx_song_write(&player,0x40,0x32);
    /* Every physical note key, despite octave, supplies the score's pitch. */
    for(slot=14;slot<41;slot++) {
        uint64_t key=UINT64_C(1)<<slot;keyboard_octave=(int)(slot%6)-3;timing_seed(60,64);
        memcpy(patch,player.registers+0x40,sizeof(patch));
        panel_edges(key,0,key);
        CHECK(player.live_note[0]==60 && keyboard_note==60 && keyboard_slot==slot && guide.hits==1);
        CHECK(!memcmp(patch,player.registers+0x40,sizeof(patch)));
        CHECK(fm1_guide_note(&guide)==64);
        CHECK(!fm1_mdx_note(&reference,60,1));
        CHECK(!fm1_mdx_render(&player,actual,128) && !fm1_mdx_render(&reference,expected,128));
        CHECK(!memcmp(actual,expected,sizeof(actual))); /* Original-patch, correct-pitch audio. */
        CHECK(!fm1_mdx_note(&reference,60,0));
        panel_edges(0,key,0);CHECK(player.live_note[0]==-1);
    }
    timing_seed(60,64);panel_edges((UINT64_C(1)<<14)|(UINT64_C(1)<<15),0,0);
    CHECK(guide.hits==1 && keyboard_slot==14 && player.live_note[0]==60);
    panel_edges(UINT64_C(1)<<15,0,0);CHECK(guide.hits==2 && player.live_note[0]==64);
    panel_edges(0,UINT64_C(1)<<14,0);CHECK(player.live_note[0]==64);
    panel_edges(0,UINT64_C(1)<<15,0);CHECK(player.live_note[0]==-1);
    timing_seed(70,72);panel_edges(UINT64_C(1)<<14,0,0);
    action(FM1_MDX_NOTE,100,1);CHECK(player.live_note[0]==72 && keyboard_slot==41 && usb_note_key==100);
    panel_edges(0,UINT64_C(1)<<14,0);action(FM1_MDX_NOTE,99,0);CHECK(player.live_note[0]==72);
    action(FM1_MDX_NOTE,100,0);CHECK(player.live_note[0]==-1 && usb_note_key==109);
    timing_seed(104,108);action(FM1_MDX_NOTE,13,1);CHECK(player.live_note[0]==104);
    action(FM1_MDX_MUTE,0,1);CHECK(usb_note_key==13); /* Idempotent mute retains owner. */
    action(FM1_MDX_NOTE,13,0);CHECK(player.live_note[0]==-1);
    guide.count=0;panel_edges(UINT64_C(1)<<14,0,0);action(FM1_MDX_NOTE,100,1);
    CHECK(player.live_note[0]==-1 && !control.error && usb_note_key==109);
    timing_seed(60,64);panel_edges(UINT64_C(1)<<14,0,0);
    action(FM1_MDX_SELECT,1,0);CHECK(player.live_note[0]==-1 && keyboard_slot==41 && usb_note_key==109 && player.mute_mask==2);
    guide_update(2048,2);CHECK(guide.active && guide.mode==2 && guide.selected==1);
    timing_seed(68,70);ui_update(10);publish();command("MDX GUIDE");
    CHECK(ui_live.guide==2 && ui_live.guide_note==-1 && !ui_live.guide_direction && ui_live.guide_pending);
    CHECK(strstr(answer,"mode=timing") && strstr(answer,"note=-1") && strstr(answer,"pending=1"));
    action(FM1_MDX_NOTE,30,1);CHECK(player.live_note[1]==68);
    action(FM1_MDX_GUIDE,FM1_GUIDE_NOTE,0);CHECK(player.live_note[1]==-1 && !guide.active && keyboard_slot==41 && usb_note_key==109);
    panel_edges(8,0,8);CHECK(control.request==FM1_MDX_GUIDE && control.a==2);control.request=0;action(FM1_MDX_GUIDE,2,0);
    panel_edges(8,0,8);CHECK(control.request==FM1_MDX_GUIDE && !control.a);control.request=0;action(FM1_MDX_GUIDE,0,0);
    panel_edges(8,0,8);CHECK(control.request==FM1_MDX_GUIDE && control.a==1);control.request=0;
    silence();keyboard_octave=0;memset(&control,0,sizeof(control));
}
static void row_address_tests(void) {
    unsigned y;screen_frame=0;
    for(y=0;y<240;y++){CHECK(!ui_row(y));CHECK(lcd_y==y);}
    CHECK(lcd_first && lcd_last);
}
static void ui_timing_tests(void) {
    unsigned i,held_level;fm1_screen_view frozen;control.running=1;ui_title_dirty=1;
    CHECK(!fm1_mdx_load(&player,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size));
    player.sequence.tracks[0].opm_volume=0;fm1_mdx_song_write(&player,8,0x78);
    ui_update(50);CHECK(ui_live.parts[0]==245 && ui_live.hold[0]==245);
    ui=ui_live;frozen=ui; /* The LCD transfer can remain busy while meters run. */
    ui_update(400);CHECK(ui_live.parts[0]>80 && ui_live.parts[0]<200 && ui_live.hold[0]==245);held_level=ui_live.parts[0];
    CHECK(!memcmp(&ui,&frozen,sizeof(ui)));
    fm1_mdx_song_write(&player,8,0);ui_update(400);CHECK(ui_live.parts[0]<held_level/2 && ui_live.hold[0]==245);
    fm1_mdx_song_write(&player,8,0x78);fm1_mdx_song_write(&player,8,0);
    ui_update(50);CHECK(ui_live.parts[0]==245 && ui_live.hold[0]==245); /* Short event is latched. */
    silence();ui_update(50);
    for(i=0;i<16;i++)CHECK(!ui_live.parts[i] && !ui_live.hold[i] && !part_motion[i].level_milli);
    for(i=0;i<32;i++)CHECK(!ui_live.spectrum[i] && !ui_live.spectrum_hold[i] && !spectrum_motion[i].level_milli);
}
int main(void){row_address_tests();panel_tests();timing_panel_tests();audio_boundary_tests();ui_timing_tests();CHECK(!fm1_peripheral_start_task());CHECK(worker);if(!setjmp(done))worker(0);
    CHECK(opened==1 && closed==1 && key_stopped==1 && lcd_stopped==1 && timer_deleted==1);
    CHECK(!audio_enabled && !scan_enabled && !alink && !keyirq && control.quiescent && !control.running);
    CHECK(fm1_peripheral_idle());puts("PASS MDX task, stereo DMA, controls, stop/upload exclusion, disconnect continuity, complete UBOOT teardown");return 0;
}
