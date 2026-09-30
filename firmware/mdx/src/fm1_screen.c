#include "fm1_screen.h"
#include <stdio.h>
#include <string.h>
/* Original renderer, shared by LCD output and the frozen screenshot. */
static uint16_t glyph(char c) {
    static const uint16_t digits[10]={0x7b6f,0x2492,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf};
    if(c>='0'&&c<='9')return digits[c-'0'];
    if(c=='B')return 0x7bae;
    if(c=='C')return 0x7927;
    switch(c){case 'A':return 0x2bed;case 'D':return 0x6b6e;case 'E':return 0x79e7;case 'F':return 0x79e4;case 'I':return 0x7497;case 'K':return 0x5bad;case 'L':return 0x4927;case 'M':return 0x7fed;case 'N':return 0x7b6d;case 'O':return 0x7b6f;case 'P':return 0x7be4;case 'R':return 0x7bad;case 'S':return 0x79cf;case 'T':return 0x7492;case 'U':return 0x5b6f;case 'X':return 0x5aad;case '-':return 0x01c0;default:return 0;}
}
static unsigned pixel(const fm1_screen_text text,unsigned x,unsigned y) {
    unsigned line=y/24,gy=(y%24)/3,gx=(x%12)/3,character=x/12;
    return line<6 && gy<5 && gx<3 && character<strlen(text[line]) &&
        (glyph(text[line][character])&(1u<<(14-gy*3-gx))) ? (line==2?2:1) : 0;
}
void fm1_screen_row(const fm1_screen_text text,unsigned y,uint8_t out[480]) {
    static const uint16_t palette[3]={0x0841,0x07ff,0xffe0};unsigned x;
    for(x=0;x<240;x++){uint16_t c=y<240?palette[pixel(text,x,y)]:palette[0];out[2*x]=(uint8_t)(c>>8);out[2*x+1]=(uint8_t)c;}
}
static uint8_t packed(const fm1_screen_text text,unsigned offset) {
    unsigned i,p=offset*4;uint8_t v=0;
    for(i=0;i<4;i++)v|=(uint8_t)(pixel(text,(p+i)%240,(p+i)/240)<<(6-2*i));
    return v;
}
static int hex8(const char *s,uint32_t *v) {
    unsigned i;*v=0;
    for(i=0;i<8;i++){unsigned d;char c=s[i];if(c>='0'&&c<='9')d=c-'0';else if(c>='a'&&c<='f')d=c-'a'+10;else if(c>='A'&&c<='F')d=c-'A'+10;else return 0;*v=(*v<<4)|d;}
    return 1;
}
static uint32_t crc(const fm1_screen_text text,void (*yield)(void *),void *u) {
    uint32_t c=0xffffffffu;unsigned i,j;
    for(i=0;i<FM1_SCREEN_BYTES;i++){c^=packed(text,i);for(j=0;j<8;j++)c=(c>>1)^(0xedb88320u&(0u-(c&1)));if(yield && i%480==479)yield(u);}
    return c^0xffffffffu;
}
int fm1_screen_command(fm1_screen *s,fm1_screen_snapshot snapshot,void (*yield)(void *),void *u,const char *command,uint32_t now,fm1_mdx_reply reply,void *ctx) {
    size_t len=strlen(command),prefix;uint32_t token,offset;char out[240];unsigned i,n;
    if(strncmp(command,"MDX SHOT",8))return 0;
    if(s->active && (uint32_t)(now-s->last_ms)>30000)s->active=0;
    if(!strcmp(command,"MDX SHOT BEGIN")) {
        s->active=0;
        if(snapshot(u,s->text,&s->frame)){reply(ctx,"ERR MDX SHOT NO_COMPLETE_FRAME\n");return 1;}
        for(i=0;i<6;i++)s->text[i][39]=0;
        if(!++s->token)++s->token;s->crc=crc(s->text,yield,u);s->last_ms=now;s->active=1;
        snprintf(out,sizeof(out),"OK MDX SHOT %08lx 240 240 I2 %08lx frame=%lu\n",(unsigned long)s->token,(unsigned long)s->crc,(unsigned long)s->frame);reply(ctx,out);return 1;
    }
    if(len==31 && !memcmp(command,"MDX SHOT READ ",14) && command[22]==' ' && hex8(command+14,&token) && hex8(command+23,&offset)) {
        if(!s->active || token!=s->token || offset>=FM1_SCREEN_BYTES || offset%FM1_SCREEN_CHUNK)goto bad;
        s->last_ms=now;n=FM1_SCREEN_BYTES-offset;if(n>FM1_SCREEN_CHUNK)n=FM1_SCREEN_CHUNK;
        snprintf(out,sizeof(out),"MDX SHOT DATA %08lx %08lx ",(unsigned long)token,(unsigned long)offset);
        prefix=strlen(out);
        for(i=0;i<n;i++){static const char digits[]="0123456789abcdef";uint8_t v=packed(s->text,offset+i);out[prefix+2*i]=digits[v>>4];out[prefix+2*i+1]=digits[v&15];}
        out[prefix+2*n]='\n';out[prefix+2*n+1]=0;reply(ctx,out);return 1;
    }
    if(len==21 && !memcmp(command,"MDX SHOT END ",13) && hex8(command+13,&token)) {
        if(!s->active || token!=s->token)goto bad;s->active=0;reply(ctx,"OK MDX SHOT END\n");return 1;
    }
bad:reply(ctx,"ERR MDX SHOT TOKEN_OFFSET_OR_COMMAND\n");return 1;
}
