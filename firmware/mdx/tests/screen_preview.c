/* Render the actual firmware UI with measured song data, without a device. */
#include "fm1_mdx.h"
#include "fm1_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static fm1_mdx_player p;
static fm1_screen_view view;
static fm1_note_motion spectrum_motion[32];
static fm1_part_motion part_motion[16];
static uint8_t *read_file(const char *name,size_t *n) {
    FILE *f=fopen(name,"rb");long len;uint8_t *b;if(!f)return 0;
    fseek(f,0,SEEK_END);len=ftell(f);rewind(f);if(len<=0||len>192*1024){fclose(f);return 0;}
    b=malloc((size_t)len);if(b&&fread(b,1,(size_t)len,f)==(size_t)len)*n=(size_t)len;else {free(b);b=0;}
    fclose(f);return b;
}
int main(int argc,char **argv) {
    size_t mn=0,pn=0;uint8_t *m,*d,row[240];int16_t audio[256];unsigned i,j,y;FILE *f;
    if(argc!=4)return 2;m=read_file(argv[1],&mn);d=read_file(argv[2],&pn);if(!m||!d)return 3;
    if(fm1_mdx_load(&p,m,mn,d,pn))return 4;
    for(i=0;i<FM1_MDX_RATE*4;i+=128){if(fm1_mdx_render(&p,audio,128))return 5;
        if(i%2176==0){uint8_t volume[16],onset[16];uint16_t held,triggered;
            for(j=0;j<32;j++){fm1_note_step(&spectrum_motion[j],p.meters.note_energy[j],49);p.meters.note_energy[j]=0;
                view.spectrum[j]=(uint8_t)(spectrum_motion[j].level_milli/1000);view.spectrum_hold[j]=(uint8_t)(spectrum_motion[j].peak_milli/1000);}
            triggered=fm1_mdx_part_activity(&p,volume,onset,&held);
            for(j=0;j<16;j++){fm1_part_step(&part_motion[j],(triggered>>j)&1,onset[j],volume[j],(held>>j)&1,49);
                view.parts[j]=(uint8_t)(part_motion[j].level_milli/1000);view.hold[j]=volume[j];}}}
    fm1_screen_title(&view,p.mdx.title.data,p.mdx.title.size);view.seconds=4;view.running=1;view.uploaded=1;view.tracks=p.mdx.track_count;
    for(j=0;j<2;j++)view.stereo[j]=fm1_meters_level(p.meters.stereo[j]);
    f=fopen(argv[3],"wb");if(!f)return 6;
    for(y=0;y<240;y++){fm1_screen_indices(&view,y,row);for(i=0;i<240;i+=2)fputc((row[i]<<4)|row[i+1],f);}
    fclose(f);free(m);free(d);return 0;
}
