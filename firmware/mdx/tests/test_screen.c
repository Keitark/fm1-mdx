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
static void title_tests(void) {
    fm1_screen_view v={0};char large[700];
    const char *lay="SUPER LAYDOCK[   Mission Striker (Opening)   ](c)T&E SOFT 1987/Ar.By Veyrlen\r\n";
    fm1_screen_title(&v,(const uint8_t *)lay,strlen(lay));
    CHECK(!strcmp(v.title,"SUPER LAYDOCK") && !strcmp(v.subtitle,"Mission Striker (Opening)") && !strcmp(v.credit,"Ar.By Veyrlen"));
    lay="OUTRUN - Magical Sound Shower - by YURAYSAN\r\n";
    fm1_screen_title(&v,(const uint8_t *)lay,strlen(lay));CHECK(!strcmp(v.credit,"by YURAYSAN"));
    lay="Song [Tune] / Composed by Person";fm1_screen_title(&v,(const uint8_t *)lay,strlen(lay));CHECK(!strcmp(v.credit,"Composed by Person"));
    lay="Song";fm1_screen_title(&v,(const uint8_t *)lay,strlen(lay));CHECK(!strcmp(v.credit,"NO EMBEDDED CREDIT"));
    lay="Song [Incomplete";fm1_screen_title(&v,(const uint8_t *)lay,strlen(lay));CHECK(!strcmp(v.subtitle,"Incomplete"));
    memset(large,'X',sizeof(large));memcpy(large,"Song / by ",10);large[20]=(char)0x80;
    fm1_screen_title(&v,(const uint8_t *)large,sizeof(large));CHECK(strlen(v.credit)==127 && v.credit[13]=='?' && !v.credit_scroll);
}
static void geometry_tests(void) {
    fm1_screen_view v={0};uint8_t row[240];unsigned y,i;
    memset(v.spectrum,255,sizeof(v.spectrum));memset(v.parts,255,sizeof(v.parts));
    /* Every column in both uniformly scaled224x49 plots has49 lit rows. */
    for(y=0;y<49;y++) {
        fm1_screen_indices(&v,79+y,row);for(i=0;i<32;i++)CHECK(row[9+i*7]!=3);
        fm1_screen_indices(&v,164+y,row);for(i=0;i<16;i++)CHECK(row[9+i*14]!=3);
    }
    for(i=0;i<28;i++) {
        unsigned lit=0;v.parts[15]=(uint8_t)((i*255)/28+1);
        for(y=164;y<213;y++){fm1_screen_indices(&v,y,row);lit+=row[219]!=3;}
        CHECK(lit==((i+1)*49+27)/28);
    }
}
static void dirty_rows(void) {
    fm1_screen_view a={0},b;uint8_t old[480],next[480],dirty[240];unsigned trial,y,field;uint32_t seed=42;
    strcpy(a.title,"OLD TITLE");strcpy(a.subtitle,"OLD SUBTITLE");strcpy(a.credit,"Ar.By Artist");
    for(trial=0;trial<420;trial++) {
        b=a;field=trial%17;seed=seed*1664525u+1013904223u;
        switch(field) {
        case 0:b.seconds=seed;break;case 1:b.running^=1;break;
        case 2:b.title[seed%32]=(char)('A'+seed%26);break;
        case 3:b.subtitle[seed%38]=(char)('A'+seed%26);break;
        case 4:b.uploaded^=1;break;case 5:b.spectrum[seed%32]=(uint8_t)seed;break;
        case 6:b.stereo[seed%2]=(uint8_t)seed;break;
        case 7:b.parts[seed%16]=(uint8_t)seed;b.hold[(seed>>8)%16]=(uint8_t)(seed>>8);break;
        case 8:b.selected=(uint8_t)(seed%8);break;case 9:b.mutes=(uint16_t)seed;break;
        case 10:b.octave=(int8_t)(seed%6-3);break;
        case 11:b.spectrum_hold[seed%32]=(uint8_t)seed;break;
        case 12:b.credit[seed%127]=(char)('A'+seed%26);break;
        case 13:b.credit_scroll=(uint16_t)(seed%600);break;
        case 14:b.guide^=1;break;case 15:b.guide_note=(int8_t)(seed%109);break;
        case 16:b.guide_direction=(int8_t)(seed%3-1);break;
        }
        fm1_screen_dirty_rows(&a,&b,dirty);
        for(y=0;y<240;y++){fm1_screen_row(&a,y,old);fm1_screen_row(&b,y,next);
            CHECK(dirty[y]==fm1_screen_row_changed(&a,&b,y));
            if(!fm1_screen_row_changed(&a,&b,y))CHECK(!memcmp(old,next,480));}
        a=b;
    }
    for(y=0;y<240;y++)CHECK(!fm1_screen_row_changed(&a,&a,y));
}
static void packed_rows(void) {
    fm1_screen_view v={0};uint8_t rgb[480],packed[362];unsigned y,x,i;
    strcpy(v.title,"RGB444 ROW PACKING");v.running=1;v.selected=5;v.mutes=0x5555;
    for(i=0;i<32;i++){v.spectrum[i]=(uint8_t)(i*8);v.spectrum_hold[i]=(uint8_t)(255-i*7);}
    for(i=0;i<16;i++){v.parts[i]=(uint8_t)(i*17);v.hold[i]=(uint8_t)(255-i*17);}
    for(y=0;y<240;y++) {
        memset(packed,0xa5,sizeof(packed));fm1_screen_row444(&v,y,packed+1);fm1_screen_row(&v,y,rgb);
        CHECK(packed[0]==0xa5 && packed[361]==0xa5);
        for(x=0;x<240;x++) {
            unsigned n=x/2*3+1,c=(rgb[x*2]<<8)|rgb[x*2+1],r,g,b;
            if(x&1){r=packed[n+1]&15;g=packed[n+2]>>4;b=packed[n+2]&15;}
            else {r=packed[n]>>4;g=packed[n]&15;b=packed[n+1]>>4;}
            CHECK(r==(c>>12) && g==((c>>7)&15) && b==((c>>1)&15));
        }
    }
}
int main(void) {
    unsigned offset,i,crc=0xffffffffu,token;uint8_t row[480];char s[80];
    title_tests();geometry_tests();dirty_rows();packed_rows();command("MDX SHOT BEGIN",0);CHECK(strstr(answer,"NO_COMPLETE_FRAME"));
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
