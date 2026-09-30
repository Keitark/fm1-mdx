#include "fm1_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static fm1_screen screen;
static fm1_screen_view shown;
static char answer[240];
static int ready;
static unsigned yields;
static void yield(void *u){(void)u;yields++;}
static int snapshot(void *u,fm1_screen_view *view,uint32_t *frame){(void)u;if(!ready)return -1;memcpy(view,&shown,sizeof(shown));*frame=7;return 0;}
static void reply(void *u,const char *s){(void)u;CHECK(strlen(s)<sizeof(answer));strcpy(answer,s);}
static void command(const char *s,uint32_t now){CHECK(fm1_screen_command(&screen,snapshot,yield,0,s,now,reply,0));}
static unsigned digit(char c){return c<='9'?c-'0':c-'a'+10;}
static void dirty_rows(void) {
    fm1_screen_view a={0},b;uint8_t old[480],next[480];unsigned trial,y,field;uint32_t seed=42;
    strcpy(a.title,"OLD TITLE");strcpy(a.subtitle,"OLD SUBTITLE");
    for(trial=0;trial<300;trial++) {
        b=a;field=trial%11;seed=seed*1664525u+1013904223u;
        switch(field) {
        case 0:b.seconds=seed;break;case 1:b.running^=1;break;
        case 2:b.title[seed%32]=(char)('A'+seed%26);break;
        case 3:b.subtitle[seed%38]=(char)('A'+seed%26);break;
        case 4:b.uploaded^=1;break;case 5:b.spectrum[seed%24]=(uint8_t)seed;break;
        case 6:b.stereo[seed%2]=(uint8_t)seed;break;
        case 7:b.parts[seed%16]=(uint8_t)seed;b.hold[(seed>>8)%16]=(uint8_t)(seed>>8);break;
        case 8:b.selected=(uint8_t)(seed%8);break;case 9:b.mutes=(uint16_t)seed;break;
        case 10:b.octave=(int8_t)(seed%6-3);break;
        }
        for(y=0;y<240;y++){fm1_screen_row(&a,y,old);fm1_screen_row(&b,y,next);
            if(!fm1_screen_row_changed(&a,&b,y))CHECK(!memcmp(old,next,480));}
        a=b;
    }
    for(y=0;y<240;y++)CHECK(!fm1_screen_row_changed(&a,&a,y));
}
int main(void) {
    unsigned offset,i,crc=0xffffffffu,token;uint8_t row[480];char s[80];
    dirty_rows();command("MDX SHOT BEGIN",0);CHECK(strstr(answer,"NO_COMPLETE_FRAME"));
    strcpy(shown.title,"SUPER LAYDOCK");shown.running=1;shown.mutes=1;shown.spectrum[4]=200;shown.parts[8]=160;ready=1;
    command("MDX SHOT BEGIN",1);token=screen.token;CHECK(screen.active && screen.frame==7 && yields==60);
    memset(&shown,0,sizeof(shown)); /* Snapshot remains immutable while UI changes. */
    for(offset=0;offset<FM1_SCREEN_BYTES;offset+=96) {
        unsigned j;
        snprintf(s,sizeof(s),"MDX SHOT READ %08x %08x",token,offset);command(s,2);CHECK(strlen(answer)==225);
        for(i=0;i<96;i++) {
            uint8_t v=(uint8_t)((digit(answer[32+2*i])<<4)|digit(answer[33+2*i]));
            unsigned p=(offset+i)*2;fm1_screen_row(&screen.view,p/240,row);
            for(j=0;j<2;j++) {
                unsigned x=(p+j)%240,c=(row[2*x]<<8)|row[2*x+1],index=(v>>(4-4*j))&15;
                CHECK(c==fm1_screen_palette[index]);
            }
            crc^=v;for(j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));
        }
    }
    CHECK((crc^0xffffffffu)==screen.crc);
    snprintf(s,sizeof(s),"MDX SHOT READ %08x 00000001",token);command(s,2);CHECK(!strncmp(answer,"ERR",3));
    snprintf(s,sizeof(s),"MDX SHOT READ %08x 00007080",token);command(s,2);CHECK(!strncmp(answer,"ERR",3));
    command("MDX SHOT BEGIN",3);CHECK(screen.token!=token);
    snprintf(s,sizeof(s),"MDX SHOT END %08x",token);command(s,3);CHECK(!strncmp(answer,"ERR",3));
    snprintf(s,sizeof(s),"MDX SHOT READ %08x 00000000",screen.token);command(s,30004);CHECK(!strncmp(answer,"ERR",3) && !screen.active);
    command("MDX SHOT READ 0000000x 00000000",30005);CHECK(!strncmp(answer,"ERR",3));
    command("MDX SHOT BEGIN trailing",30005);CHECK(!strncmp(answer,"ERR",3));
    command("MDX SHOT BEGIN",30006);snprintf(s,sizeof(s),"MDX SHOT END %08x",screen.token);command(s,30007);CHECK(!screen.active && !strncmp(answer,"OK",2));
    puts("PASS coherent screenshot, LCD pixel agreement, CRC, tokens, bounds, timeout and completion");return 0;
}
