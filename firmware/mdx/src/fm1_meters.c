#include "fm1_meters.h"
#include <string.h>
static uint32_t falling(uint32_t value,unsigned rate,uint32_t ms) {
    /* All supported tails expire within60s. Bound multiplication even after
       a long pause or a wrapped millisecond clock. */
    uint32_t loss=rate*(ms>60000?60000:ms);
    return value>loss?value-loss:0;
}
void fm1_meter_step(fm1_meter_motion *m,uint8_t input,uint32_t elapsed_ms,
                    unsigned release_per_s,unsigned hold_ms,unsigned peak_per_s) {
    uint32_t value=(uint32_t)input*1000u,peak_elapsed=elapsed_ms;
    m->level_milli=falling(m->level_milli,release_per_s,elapsed_ms);
    if(value>m->level_milli)m->level_milli=value; /* immediate attack */
    if(peak_elapsed<m->hold_ms){m->hold_ms-=peak_elapsed;peak_elapsed=0;}
    else {peak_elapsed-=m->hold_ms;m->hold_ms=0;}
    m->peak_milli=falling(m->peak_milli,peak_per_s,peak_elapsed);
    if(value && value>=m->peak_milli){m->peak_milli=value;m->hold_ms=hold_ms;}
    if(m->peak_milli<m->level_milli)m->peak_milli=m->level_milli;
}
static unsigned meter_steps(uint8_t v){return (v*28u+254)/255;}
unsigned fm1_meter_ticks(uint32_t *phase,uint32_t elapsed_ms) {
    uint32_t time;
    /* A long stopped interval expires all tails; bound32-bit multiplication. */
    if(elapsed_ms>=60000){*phase=0;return 0;}
    time=*phase+elapsed_ms*FM1_METER_HZ100;
    *phase=time%FM1_METER_PHASE_SCALE;
    return time/FM1_METER_PHASE_SCALE;
}
static unsigned decay_tick(unsigned *counter,unsigned level,unsigned speed) {
    unsigned old=*counter;*counter=(old-level)&255u;
    if(old>level)return level;
    *counter=(*counter+speed)&255u;if(*counter&128u)*counter=0;
    return level?level-1:0;
}
void fm1_part_step(fm1_part_motion *m,unsigned triggered,uint8_t onset,
                   uint8_t volume,unsigned held,uint32_t elapsed_ms) {
    unsigned ticks,counter=m->counter,level=m->step,limit=meter_steps(volume);
    if(elapsed_ms>=60000){level=0;m->tick_phase=0;}
    else {
        for(ticks=fm1_meter_ticks(&m->tick_phase,elapsed_ms);ticks && level;ticks--) {
            if(held)level=decay_tick(&counter,level,60);
            else {if(!m->off_phase)level--;m->off_phase^=1;}
        }
    }
    if(triggered){level=meter_steps(onset);counter=60;m->off_phase=0;}
    if(level>limit)level=limit;
    m->step=(uint8_t)level;m->counter=(uint8_t)counter;m->level_milli=level*255000u/28;
}
/* Mathematical level thresholds and half-level integration energy from
   MMDSP's standard display. Original C state machine, no68000 code reused. */
static const uint16_t note_threshold[29]={0,1,4,9,16,24,35,47,61,77,94,113,133,155,179,204,230,258,287,317,348,381,415,450,486,523,561,600,65535};
static const uint16_t note_half[29]={0,0,1,2,4,6,9,12,16,20,25,30,36,42,49,56,64,72,81,90,100,108,121,132,144,156,169,182,196};
static unsigned note_level(uint32_t energy) {
    unsigned level=0;if(energy>65535)energy=65535;
    while(level<28 && energy>note_threshold[level])level++;
    return level;
}
void fm1_note_step(fm1_note_motion *m,uint32_t energy,uint32_t elapsed_ms) {
    unsigned ticks,level=m->step,counter=m->counter;
    if(elapsed_ms>=60000){memset(m,0,sizeof(*m));level=counter=0;elapsed_ms=0;}
    if(energy>65535)energy=65535;
    if(energy){unsigned target=note_level(energy+note_half[level]);if(target>=level)m->target=(uint8_t)target;}
    ticks=fm1_meter_ticks(&m->tick_phase,elapsed_ms);
    while(ticks--) {
        if(m->target>level){unsigned difference=m->target-level;level+=(difference+1)/2;
            if(level>=m->target){m->target=0;counter=0;}
            if(level>m->peak){m->peak=(uint8_t)level;m->peak_count=60;}
        }else if(m->target<level)level=decay_tick(&counter,level,24);
        else m->target=0;
        /* The maximum drops every6 ticks while a bar remains, then every2.
           Equal/lower arrivals do not restart its60-tick hold. */
        if(m->peak_count)m->peak_count--;
        else {m->peak_count=(uint8_t)(level?5:1);if(m->peak>level)m->peak--;}
    }
    m->step=(uint8_t)level;m->counter=(uint8_t)counter;
    m->level_milli=level*255000u/28;m->peak_milli=m->peak*255000u/28;
}
static const int16_t tw_re[128]={32767,32757,32728,32678,32609,32521,32412,32285,32137,31971,31785,31580,31356,31113,30852,30571,30273,29956,29621,29268,28898,28510,28105,27683,27245,26790,26319,25832,25329,24811,24279,23731,23170,22594,22005,21403,20787,20159,19519,18868,18204,17530,16846,16151,15446,14732,14010,13279,12539,11793,11039,10278,9512,8739,7962,7179,6393,5602,4808,4011,3212,2410,1608,804,0,-804,-1608,-2410,-3212,-4011,-4808,-5602,-6393,-7179,-7962,-8739,-9512,-10278,-11039,-11793,-12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,-18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,-23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,-27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,-30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,-32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757};
static const int16_t tw_im[128]={0,-804,-1608,-2410,-3212,-4011,-4808,-5602,-6393,-7179,-7962,-8739,-9512,-10278,-11039,-11793,-12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,-18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,-23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,-27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,-30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,-32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,-32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,-32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,-30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,-27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,-23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,-18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,-12539,-11793,-11039,-10278,-9512,-8739,-7962,-7179,-6393,-5602,-4808,-4011,-3212,-2410,-1608,-804};
static const int16_t window[256]={0,5,20,44,79,123,177,241,315,398,491,593,705,827,958,1098,1247,1406,1573,1749,1935,2128,2331,2542,2761,2989,3224,3468,3719,3978,4244,4518,4799,5086,5381,5682,5990,6304,6624,6950,7281,7618,7961,8308,8660,9017,9379,9744,10114,10487,10864,11244,11628,12014,12403,12794,13187,13583,13980,14378,14778,15178,15580,15981,16383,16786,17187,17589,17989,18389,18787,19184,19580,19973,20364,20753,21139,21523,21903,22280,22653,23023,23388,23750,24107,24459,24806,25149,25486,25817,26143,26463,26777,27085,27386,27681,27968,28249,28523,28789,29048,29299,29543,29778,30006,30225,30436,30639,30832,31018,31194,31361,31520,31669,31809,31940,32062,32174,32276,32369,32452,32526,32590,32644,32688,32723,32747,32762,32767,32762,32747,32723,32688,32644,32590,32526,32452,32369,32276,32174,32062,31940,31809,31669,31520,31361,31194,31018,30832,30639,30436,30225,30006,29778,29543,29299,29048,28789,28523,28249,27968,27681,27386,27085,26777,26463,26143,25817,25486,25149,24806,24459,24107,23750,23388,23023,22653,22280,21903,21523,21139,20753,20364,19973,19580,19184,18787,18389,17989,17589,17187,16786,16384,15981,15580,15178,14778,14378,13980,13583,13187,12794,12403,12014,11628,11244,10864,10487,10114,9744,9379,9017,8660,8308,7961,7618,7281,6950,6624,6304,5990,5682,5381,5086,4799,4518,4244,3978,3719,3468,3224,2989,2761,2542,2331,2128,1935,1749,1573,1406,1247,1098,958,827,705,593,491,398,315,241,177,123,79,44,20,5};

void fm1_meters_peak(uint16_t *peak,int32_t sample) {
    uint32_t a=sample<0?(uint32_t)(-(int64_t)sample):(uint32_t)sample;
    if(a>32768)a=32768;if(a>*peak)*peak=(uint16_t)a;
}
void fm1_meters_feed(fm1_meters *m,int16_t l,int16_t r) {
    fm1_meters_peak(&m->stereo[0],l);fm1_meters_peak(&m->stereo[1],r);
    if(m->count<FM1_FFT_SIZE){m->samples[m->count][0]=l;m->samples[m->count][1]=r;m->count++;}
}
uint8_t fm1_meters_level(uint32_t a) {
    unsigned log=0;uint32_t base,value;
    if(a<=32)return 0;if(a>=32768)return 255;
    base=a;while(base>>1){base>>=1;log++;}
    base=1u<<log;value=(log-5)*256+(a-base)*256/base;
    return (uint8_t)(value/10);
}
static uint32_t root(uint32_t x) {
    uint32_t b=1u<<30,r=0;while(b>x)b>>=2;
    while(b){if(x>=r+b){x-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;
}
void fm1_meters_note(fm1_meters *m,unsigned midi,uint8_t velocity) {
    unsigned d,weight[6];int center;
    if(midi<13 || midi>125)return;center=(int)((midi-13)/3);
    if(velocity>127)velocity=127;
    weight[0]=velocity;weight[1]=(velocity>>1)+(velocity>>2);
    weight[2]=(weight[1]-(velocity>>3))>>1;weight[3]=velocity>>3;
    weight[4]=velocity>>2;weight[5]=velocity>>4;
    /* Three semitones per band; the secondary shoulder at distance4 is
       intentional. Out-of-range pitches may spill into visible neighbors. */
    for(d=0;d<6;d++){int side;for(side=-1;side<=1;side+=2){
        int band=center+side*(int)d;uint32_t total;
        if(!d && side==1)continue;if(band<0 || band>=32)continue;
        total=m->note_energy[band];m->note_energy[band]=total>65535u-weight[d]?65535u:total+weight[d];
    }}
}
void fm1_meters_note_spectrum(fm1_meters *m,uint8_t out[FM1_NOTE_BARS]) {
    unsigned i;for(i=0;i<32;i++){out[i]=(uint8_t)(note_level(m->note_energy[i])*255/28);m->note_energy[i]=0;}
}
void fm1_meters_spectrum(fm1_meters *m,uint8_t out[FM1_SPECTRUM_BARS]) {
    static int32_t re[256],im[256]; /* owner-task workspace, no stack spike */
    static const unsigned edges[25]={1,2,3,4,5,6,7,8,9,11,13,16,19,23,28,34,41,50,60,72,85,99,113,122,129};
    uint32_t bands[24]={0};unsigned channel,i,j,len,bar;
    if(m->count<256)return;
    for(channel=0;channel<2;channel++) {
        for(i=0;i<256;i++) {
            unsigned rev=0,k=i,b;for(b=0;b<8;b++){rev=(rev<<1)|(k&1);k>>=1;}
            re[rev]=((int32_t)m->samples[i][channel]*window[i])/32768;im[rev]=0;
        }
        for(len=2;len<=256;len*=2)for(i=0;i<256;i+=len)for(j=0;j<len/2;j++) {
            unsigned a=i+j,b=a+len/2,t=j*256/len;
            /* Divide each product before addition: bounded32-bit intermediates.
               Each butterfly scales by2, for an overall1/N transform. */
            int32_t tr=re[b]*tw_re[t]/32768-im[b]*tw_im[t]/32768;
            int32_t ti=re[b]*tw_im[t]/32768+im[b]*tw_re[t]/32768;
            int32_t ar=re[a],ai=im[a];
            re[a]=(ar+tr)/2;im[a]=(ai+ti)/2;re[b]=(ar-tr)/2;im[b]=(ai-ti)/2;
        }
        for(bar=0;bar<24;bar++)for(i=edges[bar];i<edges[bar+1];i++) {
            uint32_t power=(uint32_t)(re[i]*re[i])+(uint32_t)(im[i]*im[i]);
            /* Nyquist has no conjugate partner: amplitude factor2, not4. */
            if(i==128)power/=4;
            if(power>bands[bar])bands[bar]=power;
        }
    }
    for(bar=0;bar<24;bar++)out[bar]=fm1_meters_level(root(bands[bar])*4u);
    m->count=0;
}
