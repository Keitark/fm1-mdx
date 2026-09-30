/* One synth/transport owner task. IRQs only scan inputs and drain PCM. */
#ifdef FM1_MDX_TARGET_HOST
#include "fake_peripherals.h"
#else
#include "app_config.h"
#include "system/includes.h"
#include "system/task.h"
#include "system/sys_time.h"
#include "system/timer.h"
#include "system/spinlock.h"
#include "os/os_api.h"
#include "asm/iis.h"
#include "asm/wdt.h"
#endif
#include "fm1_mdx.h"
#include "fm1_mdx_usb.h"
#include "fm1_screen.h"
#include "fm1_wl82_keyscan.h"
#include "fm1_volume.h"
#include "peripheral_logic.h"
#include "peripherals.h"
#include "display_test.h"
#ifdef FM1_USB_AUDIO
#include "target.h"
#endif
#include <stdio.h>
#include <string.h>
extern const unsigned char fm1_demo_mdx[],fm1_demo_pdx[];
extern const size_t fm1_demo_mdx_size,fm1_demo_pdx_size;
static fm1_mdx_player player;
static fm1_mdx_upload upload;
static fm1_wl82_keyscan scanner;
static fm1_volume mdx_volume;
static fm1_encoders encoders;
static fm1_audio_startup envelope;
static spinlock_t control_lock,audio_lock,input_lock;
static struct {unsigned request,a,b,running,available,selected,mutes,frames,error,underruns,shutdown,quiescent,panel;int octave,last_panel;} control;
static int keyboard_octave;
static unsigned keyboard_slot=41,keyboard_note;
static int16_t ring[2048][2],block[FM1_MDX_BLOCK*2];
static uint32_t rd,wr,underruns,frames;
static uint32_t callback_ms,callback_gap_ms,late_callbacks,rebuffer_events,rebuffer_frames,render_max_ms;
static unsigned callback_seen,queue_min=2048;
static unsigned primed,audio_enabled,audio_playing,scan_enabled,scan_ticks;
static uint8_t row_pixels[480];
static fm1_screen_view screen_shown;
static uint32_t ui_rows,ui_skipped,ui_max_ms,ui_fps10,ui_epoch,ui_epoch_frames;
static uint8_t previous_pixels[480];
static uint32_t screen_frame;
static fm1_screen screen_transfer;
static volatile uint32_t mdx_lcd_registers[20],mdx_lcd_phase;
static volatile int lcd_error,key_error,audio_error;
static int scan_timer,uploaded_song;
static unsigned take(spinlock_t *l){unsigned f;local_irq_save(f);arch_spin_lock(l);return f;}
static void release(spinlock_t *l,unsigned f){arch_spin_unlock(l);local_irq_restore(f);}
static int screen_snapshot(void *u,fm1_screen_view *view,uint32_t *frame) {
    unsigned f=take(&control_lock);int rc;(void)u;
    rc=!control.available || control.shutdown || lcd_error || !screen_frame;
    if(!rc){memcpy(view,&screen_shown,sizeof(screen_shown));*frame=screen_frame;}
    release(&control_lock,f);return rc;
}
static void screen_yield(void *u){(void)u;os_time_dly(1);}
static uint32_t now_us(void *u){(void)u;return timer_get_ms()*1000u;}
static int idle(void *u){(void)u;return control.available && !control.running && !control.request;}
static int request(void *u,unsigned op,unsigned a,unsigned b) {
    (void)u;
    if(!control.available || upload.active || (control.request && op!=FM1_MDX_STOP))return -1;
    if(op>=FM1_MDX_SELECT && !control.running)return -1;
    control.request=op;control.a=a;control.b=b;return 0;
}
static void status(void *u,char *out,size_t n) {
    (void)u;
    snprintf(out,n,"MDX running=%u pending=%u selected=%u mute=%04x frames=%u underruns=%u error=%u upload=%u ready=%u lcd=%d keys=%d audio=%d oct=%d panel=%04x last=%d\n",
             control.running,control.request,control.selected,control.mutes,control.frames,control.underruns,control.error,upload.active,upload.ready,lcd_error,key_error,audio_error,control.octave,control.panel,control.last_panel);
}
int fm1_mdx_usb_command(const char *s,uint32_t now,fm1_mdx_reply reply,void *ctx) {
    fm1_mdx_usb_io io={0,idle,request,status};int result;unsigned f;
    if(fm1_screen_command(&screen_transfer,screen_snapshot,screen_yield,0,s,now,reply,ctx))return 1;
#ifdef FM1_USB_AUDIO
    if(!strcmp(s,"MDX AUDIO")){char out[224];fm1_usb_audio_status(out,sizeof(out));reply(ctx,out);return 1;}
#endif
    if(!strcmp(s,"MDX VOLUME")) {
        char out[200];fm1_volume v;unsigned gain;
        f=take(&input_lock);v=mdx_volume;release(&input_lock,f);
        f=take(&audio_lock);gain=envelope.gain_q7;release(&audio_lock,f);
        snprintf(out,sizeof(out),"MDX VOLUME running=%u valid=%u raw=%u target=%u gain=%u samples=%lu errors=%lu\n",
            v.running,v.valid,v.raw,v.target,gain,(unsigned long)v.samples,(unsigned long)v.errors);reply(ctx,out);return 1;
    }
    if(!strcmp(s,"MDX SCAN")) {
        char out[224];fm1_wl82_keyscan_failure d;unsigned enabled,completions;
        f=take(&input_lock);d=scanner.failure;enabled=scan_enabled;completions=scanner.completions;release(&input_lock,f);
        snprintf(out,sizeof(out),"MDX SCAN enabled=%u completions=%u reason=%lu row=%lu elapsed_us=%lu con=%08lx cnt=%lu\n",
            enabled,completions,(unsigned long)d.reason,(unsigned long)d.row,(unsigned long)(d.end_us-d.start_us),
            (unsigned long)d.con,(unsigned long)d.dma_count);reply(ctx,out);return 1;
    }
    if(!strcmp(s,"MDX INPUT")) {
        char out[224];size_t n;
        f=take(&control_lock);
        n=(size_t)snprintf(out,sizeof(out),"MDX INPUT oct=%d panel=%04x last=%d",control.octave,control.panel,control.last_panel);
        release(&control_lock,f);f=take(&input_lock);
        snprintf(out+n,sizeof(out)-n," enc=%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
                 (long)encoders.count[0],(long)encoders.count[1],(long)encoders.count[2],(long)encoders.count[3],(long)encoders.count[4],(long)encoders.count[5],(long)encoders.count[6]);
        release(&input_lock,f);reply(ctx,out);return 1;
    }
    if(!strcmp(s,"MDX DISPLAY")) {
        char out[200];unsigned f=take(&control_lock);
        snprintf(out,sizeof(out),"MDX DISPLAY frame=%lu fps10=%lu rows=%lu skipped=%lu max_ms=%lu target_fps=20\n",
            (unsigned long)screen_frame,(unsigned long)ui_fps10,(unsigned long)ui_rows,(unsigned long)ui_skipped,(unsigned long)ui_max_ms);
        release(&control_lock,f);reply(ctx,out);return 1;
    }
    if(!strcmp(s,"MDX TIMING")) {
        char out[200];unsigned gap,late,minimum,events,missing,render;
        f=take(&audio_lock);gap=callback_gap_ms;late=late_callbacks;minimum=queue_min;
        events=rebuffer_events;missing=rebuffer_frames;render=render_max_ms;release(&audio_lock,f);
        snprintf(out,sizeof(out),"MDX TIMING cb_gap_ms=%u late=%u min_fill=%u rebuffer=%u rebuffer_frames=%u render_ms=%u\n",gap,late,minimum,events,missing,render);
        reply(ctx,out);return 1;
    }
    f=take(&control_lock);
    result=fm1_mdx_usb_line(&upload,&io,s,now,reply,ctx);release(&control_lock,f);return result;
}
void fm1_mdx_usb_reset(void){unsigned f=take(&control_lock);fm1_mdx_usb_abort(&upload);release(&control_lock,f);screen_transfer.active=0;}
void fm1_mdx_usb_tick(uint32_t now){unsigned f=take(&control_lock);fm1_mdx_usb_timeout(&upload,now);release(&control_lock,f);}
void fm1_peripheral_cancel(void){unsigned f=take(&control_lock);if(!control.quiescent){control.request=FM1_MDX_STOP;control.shutdown=1;}release(&control_lock,f);}
void fm1_peripheral_session_cancel(void){fm1_mdx_usb_reset();} /* Standalone playback survives disconnect. */
int fm1_peripheral_idle(void){unsigned f=take(&control_lock);int v=control.quiescent;release(&control_lock,f);return v;}
int fm1_peripheral_request(unsigned command,unsigned generation){unsigned f;int rc;(void)generation;if(command!=1)return -1;f=take(&control_lock);rc=request(0,FM1_MDX_STOP,0,0);release(&control_lock,f);return rc;}
int fm1_peripheral_event(char *out,unsigned n){(void)out;(void)n;return 0;}
__attribute__((noinline,used))
void fm1_display_snapshot(unsigned phase,const uint32_t *r){unsigned i;for(i=0;i<20;i++)mdx_lcd_registers[i]=r[i];mdx_lcd_phase=phase;}
static void audio_output(void *ctx,u8 *data,int len,u8 ch) {
    unsigned i,count;int32_t *out=(int32_t *)data;uint32_t available=wr-rd,now=timer_get_ms();
    (void)ctx;
    if(!data || len<=0)return;
    if(ch!=3 || len!=512){memset(data,0,(unsigned)len);audio_error=-1;return;}
    /* The pinned SDK exposes jiffies*10, not a1ms clock. A normal callback
       crossing a tick reads10ms; only two or more ticks are suspicious. */
    if(callback_seen){unsigned gap=now-callback_ms;if(gap>callback_gap_ms)callback_gap_ms=gap;if(gap>=20)late_callbacks++;}
    callback_ms=now;callback_seen=1;
    if(!primed && available>=1470){primed=1;audio_playing=1;}
    if(primed && available<queue_min)queue_min=available;
    count=primed?(available<64?available:64):0;
    for(i=0;i<64;i++) {
        if(i<count){out[2*i]=(int32_t)ring[rd&2047][0]*256;out[2*i+1]=(int32_t)ring[rd&2047][1]*256;rd++;}
        else {out[2*i]=out[2*i+1]=0;if(primed)underruns++;}
    }
    /* A completely consumed block is still successful. Unprime only when
       a callback actually lacks samples, not when the producer can refill
       before the next callback. The old exact-empty case inserted an
       uncounted 33ms priming gap despite underruns=0. */
    if(primed && count<64){primed=0;audio_playing=2;rebuffer_events++;}
    if(!primed && audio_playing==2)rebuffer_frames+=64-count;
    {unsigned f=take(&input_lock);envelope.target_q7=mdx_volume.valid?mdx_volume.target:0;release(&input_lock,f);}
#ifdef FM1_USB_AUDIO
    fm1_usb_audio_dac(out,64);
#endif
    fm1_audio_startup_process24(&envelope,out);frames+=64;
}
___interrupt
static void fm1_test_alink_isr(void){unsigned f=take(&audio_lock);if(audio_enabled)iis_irq_handler(0);release(&audio_lock,f);}
___interrupt
static void fm1_mdx_keyscan_isr(void){unsigned f=take(&input_lock);if(scan_enabled)fm1_wl82_keyscan_async_step(&scanner);release(&input_lock,f);}
static void fm1_mdx_scan_tick(void *u) {
    unsigned f=take(&input_lock);(void)u;
    /* ADC volume and matrix scanning share a timer, not a failure lifetime. */
    if(!(++scan_ticks&1))fm1_volume_tick(&mdx_volume);
    if(scan_enabled)fm1_wl82_keyscan_async_kick(&scanner);
    release(&input_lock,f);
}
static void publish(void) {
    unsigned f,fr,ur;f=take(&audio_lock);fr=frames;ur=underruns;release(&audio_lock,f);
    f=take(&control_lock);control.selected=player.selected;control.mutes=player.mute_mask;control.frames=fr;control.underruns=ur;control.error=(unsigned)player.error;release(&control_lock,f);
}
static void silence(void) {
    unsigned f=take(&audio_lock);rd=wr=0;primed=0;audio_playing=0;release(&audio_lock,f);
    fm1_mdx_stop(&player);
    keyboard_slot=41;
    f=take(&control_lock);control.running=0;release(&control_lock,f);
}
static void panel_edges(uint64_t down,uint64_t up,uint64_t held) {
    unsigned f,i,running;
    /* Panel order: OCT-/OCT+, FX/SEL/ENV/LFO/EDIT/GLO,
       HOME/SAVE/ARP/SEQ/PLAY-STOP/REC. Slot labels are bench-checkable below. */
    if((down&3)==1 && keyboard_octave>-3)keyboard_octave--;
    else if((down&3)==2 && keyboard_octave<2)keyboard_octave++;
    f=take(&control_lock);running=control.running;
    control.octave=keyboard_octave;control.panel=(unsigned)(held&0x3fff);
    for(i=0;i<14;i++)if((down>>i)&1)control.last_panel=(int)i;
    if(!upload.active && !control.request) {
        if(down&(UINT64_C(1)<<12))request(0,running?FM1_MDX_STOP:(uploaded_song&&upload.ready?FM1_MDX_PLAY:FM1_MDX_DEMO),0,0);
        else if(down&4)request(0,FM1_MDX_MUTE,player.selected,!(player.mute_mask&(1u<<player.selected)));
    }
    release(&control_lock,f);
    if(!running)return;
    /* Release the pitch captured on key-down, even after an octave change.
       An older key release must not silence the latest monophonic owner. */
    if(keyboard_slot<41 && ((up>>keyboard_slot)&1)) {
        fm1_mdx_note(&player,keyboard_note,0);keyboard_slot=41;
    }
    for(i=14;i<41;i++)if((down>>i)&1) {
        unsigned note=(unsigned)(53+(int)i-14+12*keyboard_octave);
        if(!fm1_mdx_note(&player,note,1)){keyboard_slot=i;keyboard_note=note;}
    }
}
static void shutdown(void) {
    unsigned f;
#ifdef FM1_USB_AUDIO
    fm1_usb_audio_stop();
#endif
    silence();
    if(scan_timer>0)sys_usec_timer_del(scan_timer);
    bit_clr_ie(IRQ_SPI2_IDX,0);f=take(&input_lock);scan_enabled=0;
    if(scanner.running)fm1_wl82_keyscan_stop(&scanner);
    fm1_volume_stop(&mdx_volume);release(&input_lock,f);unrequest_irq(IRQ_SPI2_IDX,0);
    bit_clr_ie(IRQ_ALNK_IDX,0);f=take(&audio_lock);audio_enabled=0;release(&audio_lock,f);
    unrequest_irq(IRQ_ALNK_IDX,0);iis_channel_off(8,0);iis_close(0);
    fm1_display_test_stop();
    f=take(&control_lock);control.available=0;control.quiescent=1;release(&control_lock,f);
    for(;;)os_time_dly(100);
}
static void action(unsigned op,unsigned a,unsigned b) {
    int rc=0;
    if(op==FM1_MDX_STOP){silence();return;}
    if(op==FM1_MDX_DEMO || op==FM1_MDX_PLAY) {
        unsigned f=take(&audio_lock);rd=wr=0;primed=0;audio_playing=1;release(&audio_lock,f);
        if(op==FM1_MDX_PLAY) {
            uploaded_song=1;
            rc=fm1_mdx_load(&player,upload.bytes+12,upload.mdx_size,upload.bytes+12+upload.mdx_size,upload.pdx_size);
        } else {
            uploaded_song=0;rc=fm1_mdx_load(&player,fm1_demo_mdx,fm1_demo_mdx_size,fm1_demo_pdx,fm1_demo_pdx_size);
            /* The generated PCM drum sounds like a metronome. Keep it muted
               in the built-in demo; external MDX/PDX songs retain their mix. */
            if(!rc)rc=fm1_mdx_mute(&player,8,1);
        }
    } else if(op==FM1_MDX_SELECT)rc=fm1_mdx_select(&player,a);
    else if(op==FM1_MDX_MUTE)rc=fm1_mdx_mute(&player,a,(int)b);
    else if(op==FM1_MDX_NOTE)rc=fm1_mdx_note(&player,a,(int)b);
    if(rc){unsigned f=take(&control_lock);control.error=(unsigned)rc;release(&control_lock,f);}
}
/* Bounded dirty-row batches. The same frozen state renders LCD and SHOT. */
static fm1_screen_view ui;
static void ui_update(void) {
    unsigned i;fm1_screen_title(&ui,player.mdx.title.data,player.mdx.title.size);
    ui.running=(uint8_t)control.running;ui.selected=player.selected;ui.mutes=player.mute_mask;
    ui.tracks=player.mdx.track_count;ui.uploaded=(uint8_t)uploaded_song;ui.octave=(int8_t)keyboard_octave;
    ui.seconds=(uint32_t)(player.cycles/RETROFM_PL_CLOCK_HZ);
    fm1_meters_spectrum(&player.meters,ui.spectrum);
    for(i=0;i<16;i++) {
        unsigned value=fm1_meters_level(player.meters.parts[i]);player.meters.parts[i]=0;
        /*50ms frame decay, roughly60dB/sec; caps decay more slowly. */
        ui.parts[i]=ui.parts[i]>13?ui.parts[i]-13:0;if(value>ui.parts[i])ui.parts[i]=(uint8_t)value;
        ui.hold[i]=ui.hold[i]>4?ui.hold[i]-4:0;if(value>ui.hold[i])ui.hold[i]=(uint8_t)value;
    }
    for(i=0;i<2;i++){ui.stereo[i]=fm1_meters_level(player.meters.stereo[i]);player.meters.stereo[i]=0;}
    if(!control.running){memset(ui.spectrum,0,sizeof(ui.spectrum));memset(&player.meters,0,sizeof(player.meters));}
}
static int ui_row(unsigned y) {
    uint8_t col[4]={0,0,0,239},rows[4]={0,(uint8_t)(40+y),0,(uint8_t)(40+y)};
    /* y+40 can exceed255; set the high bytes explicitly. */
    rows[0]=rows[2]=(uint8_t)((40+y)>>8);
    fm1_screen_row(&ui,y,row_pixels);
    if(screen_frame){fm1_screen_row(&screen_shown,y,previous_pixels);if(!memcmp(row_pixels,previous_pixels,480)){ui_skipped++;return 0;}}
    ui_rows++;
    {uint8_t cmd=0x2a;if(fm1_display_write(0,&cmd,1)||fm1_display_write(1,col,4))return -1;cmd=0x2b;if(fm1_display_write(0,&cmd,1)||fm1_display_write(1,rows,4))return -1;cmd=0x2c;if(fm1_display_write(0,&cmd,1)||fm1_display_write(1,row_pixels,480))return -1;}
    return 0;
}
__attribute__((noinline,used))
static void fm1_peripheral_task(void *u) {
    struct iis_platform_data pd;unsigned f;uint32_t last_ui=0,last_status=0,changed=0,ui_started=0;unsigned row=240;
    uint64_t candidate=0,stable=0;int32_t encoder=0;
    (void)u;memset(&pd,0,sizeof(pd));
    lcd_error=fm1_display_test_init();
    bit_clr_ie(IRQ_SPI2_IDX,0);key_error=fm1_wl82_keyscan_async_start(&scanner,0,now_us);
    fm1_volume_start(&mdx_volume);
    if(!key_error) {
        scan_enabled=1;request_irq(IRQ_SPI2_IDX,5,fm1_mdx_keyscan_isr,0);
    }
    scan_timer=sys_usec_timer_add(0,fm1_mdx_scan_tick,1000,1,0);
    if(scan_timer<=0){key_error=-1;scan_enabled=0;fm1_volume_stop(&mdx_volume);}
    fm1_audio_startup_reset(&envelope);
    pd.port_sel=IIS_PORTC;pd.channel_out=pd.data_width=8;pd.mclk_output=1;pd.sr_points=128;
    audio_error=iis_open(&pd,0);
    if(!audio_error){iis_set_dec_data_handler(0,audio_output,0);audio_error=iis_set_sample_rate(FM1_MDX_RATE,0);}
    if(!audio_error){audio_enabled=1;request_irq(IRQ_ALNK_IDX,3,fm1_test_alink_isr,0);iis_channel_on(8,0);}
    f=take(&control_lock);control.available=1;control.running=1;control.last_panel=-1;release(&control_lock,f);
    action(FM1_MDX_DEMO,0,0);
    for(;;) {
        unsigned op,a,b,running,queued,closing;uint32_t now=timer_get_ms();uint8_t rows[11];int scan_rc;
        f=take(&control_lock);op=control.request;a=control.a;b=control.b;control.request=0;
        if(op==FM1_MDX_PLAY || op==FM1_MDX_DEMO)control.running=1;
        running=control.running;closing=control.shutdown;release(&control_lock,f);
        if(closing)shutdown();
        if(op){action(op,a,b);publish();if(op==FM1_MDX_STOP)running=0;}
        f=take(&audio_lock);queued=wr-rd;release(&audio_lock,f);
        if(running && player.loaded && !audio_error && queued<=2048-FM1_MDX_BLOCK) {
            uint32_t started=timer_get_ms(),elapsed;
            if(fm1_mdx_render(&player,block,FM1_MDX_BLOCK) || !player.playing){silence();running=0;}
            else {
                unsigned i;f=take(&audio_lock);
                for(i=0;i<FM1_MDX_BLOCK;i++){ring[wr&2047][0]=block[2*i];ring[wr&2047][1]=block[2*i+1];wr++;}
                release(&audio_lock,f);
            }
            elapsed=timer_get_ms()-started;f=take(&audio_lock);if(elapsed>render_max_ms)render_max_ms=elapsed;release(&audio_lock,f);
        }
        f=take(&input_lock);scan_rc=scan_enabled?fm1_wl82_keyscan_async_raw(&scanner,rows):FM1_NES_BUSY;release(&input_lock,f);
        if(!scan_rc) {
            uint64_t keys=fm1_stock_decode_keys(rows),down,up;
            f=take(&input_lock);fm1_encoders_sample(&encoders,rows);release(&input_lock,f);
            if(keys!=candidate){candidate=keys;changed=now;}
            if((uint32_t)(now-changed)>=10 && candidate!=stable) {
                down=candidate&~stable;up=stable&~candidate;stable=candidate;
                panel_edges(down,up,stable);
            }
            if(encoders.count[0]!=encoder) {
                int step=encoders.count[0]>encoder?1:-1;encoder=encoders.count[0];
                f=take(&control_lock);if(!upload.active && !control.request)request(0,FM1_MDX_SELECT,(player.selected+8+step)%8,0);release(&control_lock,f);
            }
        } else if(scan_rc!=FM1_NES_BUSY && scan_enabled) {
            key_error=scan_rc;f=take(&input_lock);scan_enabled=0;fm1_wl82_keyscan_stop(&scanner);release(&input_lock,f);
            /* Lost key releases must never leave a manual voice held. */
            for(a=0;a<8;a++)if(player.live_note[a]>=0){fm1_mdx_song_write(&player,8,(uint8_t)a);ym2151_write_reg(&player.opm,8,(int)a);player.live_note[a]=-1;}
        }
        if((uint32_t)(now-last_status)>=250){last_status=now;publish();}
        /* Rendering changes occupancy; use the live reserve for UI/sleep. */
        f=take(&audio_lock);queued=wr-rd;release(&audio_lock,f);
        if(!lcd_error && (queued>=1470 || !running)) {
            unsigned batch;
            if(row==240 && (uint32_t)(now-last_ui)>=50){last_ui=now;ui_started=timer_get_ms();ui_update();row=0;}
            for(batch=0;row<240 && batch<4;batch++) {
                /* DMA waits leave IRQs enabled. Recheck reserve before each row. */
                f=take(&audio_lock);queued=wr-rd;release(&audio_lock,f);
                if(running && queued<1470)break;
                lcd_error=ui_row(row++);if(lcd_error)break;
                if(row==240) {
                    uint32_t elapsed=timer_get_ms()-ui_started;
                    f=take(&control_lock);screen_shown=ui;if(!++screen_frame)++screen_frame;
                    if(elapsed>ui_max_ms)ui_max_ms=elapsed;
                    if(!ui_epoch){ui_epoch=timer_get_ms();ui_epoch_frames=screen_frame;}
                    if(timer_get_ms()-ui_epoch>=1000){ui_fps10=(screen_frame-ui_epoch_frames)*10000u/(timer_get_ms()-ui_epoch);ui_epoch=timer_get_ms();ui_epoch_frames=screen_frame;}
                    release(&control_lock,f);
                }
            }
        }
        /* Do not pay a10ms RTOS tick per row. Refill/control between batches;
           sleep after a completed frame, when idle, or after LCD/audio failure. */
        wdt_clear();
        f=take(&audio_lock);queued=wr-rd;release(&audio_lock,f);
        if((row==240 || lcd_error || audio_error) && (queued>=1470 || !running || audio_error))os_time_dly(1);
    }
}
int fm1_peripheral_start_task(void){return task_create(fm1_peripheral_task,0,"peripheral");}
