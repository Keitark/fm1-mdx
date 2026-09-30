#include "fm1_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static fm1_screen screen;
static fm1_screen_text shown;
static char answer[240];
static int ready;
static unsigned yields;
static void yield(void *u){(void)u;yields++;}
static int snapshot(void *u,fm1_screen_text text,uint32_t *frame){(void)u;if(!ready)return -1;memcpy(text,shown,sizeof(shown));*frame=7;return 0;}
static void reply(void *u,const char *s){(void)u;CHECK(strlen(s)<sizeof(answer));strcpy(answer,s);}
static void command(const char *s,uint32_t now){CHECK(fm1_screen_command(&screen,snapshot,yield,0,s,now,reply,0));}
static unsigned digit(char c){return c<='9'?c-'0':c-'a'+10;}
int main(void) {
    unsigned offset,i,crc=0xffffffffu,token;uint8_t row[480];char s[80];
    command("MDX SHOT BEGIN",0);CHECK(strstr(answer,"NO_COMPLETE_FRAME"));
    snprintf(shown[0],40,"FM1 MDX PLAY");snprintf(shown[2],40,"TRACK 1 KARAOKE");ready=1;
    command("MDX SHOT BEGIN",1);token=screen.token;CHECK(screen.active && screen.frame==7 && yields==30);
    memset(shown,0,sizeof(shown)); /* Snapshot remains immutable while UI changes. */
    for(offset=0;offset<FM1_SCREEN_BYTES;offset+=96) {
        unsigned j;
        snprintf(s,sizeof(s),"MDX SHOT READ %08x %08x",token,offset);command(s,2);CHECK(strlen(answer)==225);
        for(i=0;i<96;i++) {
            uint8_t v=(uint8_t)((digit(answer[32+2*i])<<4)|digit(answer[33+2*i]));
            unsigned p=(offset+i)*4;fm1_screen_row(screen.text,p/240,row);
            for(j=0;j<4;j++) {
                unsigned x=(p+j)%240,c=(row[2*x]<<8)|row[2*x+1],index=(v>>(6-2*j))&3;
                CHECK(c==(index==0?0x0841:index==1?0x07ff:0xffe0) && index<3);
            }
            crc^=v;for(j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));
        }
    }
    CHECK((crc^0xffffffffu)==screen.crc);
    snprintf(s,sizeof(s),"MDX SHOT READ %08x 00000001",token);command(s,2);CHECK(!strncmp(answer,"ERR",3));
    snprintf(s,sizeof(s),"MDX SHOT READ %08x 00003840",token);command(s,2);CHECK(!strncmp(answer,"ERR",3));
    command("MDX SHOT BEGIN",3);CHECK(screen.token!=token);
    snprintf(s,sizeof(s),"MDX SHOT END %08x",token);command(s,3);CHECK(!strncmp(answer,"ERR",3));
    snprintf(s,sizeof(s),"MDX SHOT READ %08x 00000000",screen.token);command(s,30004);CHECK(!strncmp(answer,"ERR",3) && !screen.active);
    command("MDX SHOT READ 0000000x 00000000",30005);CHECK(!strncmp(answer,"ERR",3));
    command("MDX SHOT BEGIN trailing",30005);CHECK(!strncmp(answer,"ERR",3));
    command("MDX SHOT BEGIN",30006);snprintf(s,sizeof(s),"MDX SHOT END %08x",screen.token);command(s,30007);CHECK(!screen.active && !strncmp(answer,"OK",2));
    puts("PASS coherent screenshot, LCD pixel agreement, CRC, tokens, bounds, timeout and completion");return 0;
}
