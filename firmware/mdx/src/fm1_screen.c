#include "fm1_screen.h"
#include <stdio.h>
#include <string.h>
#define RGB(r,g,b) (((r)>>3)<<11|((g)>>2)<<5|((b)>>3))
/* Pocket-inspired violet palette. Client expands these exact565 values. */
const uint16_t fm1_screen_palette[16]={
    RGB(4,4,16),RGB(10,10,30),RGB(55,55,100),RGB(20,20,45),
    RGB(30,30,70),RGB(210,210,235),RGB(150,150,180),RGB(210,210,245),
    RGB(245,245,255),RGB(45,30,110),RGB(70,70,140),RGB(110,110,180),
    RGB(150,150,210),RGB(70,80,150),RGB(150,170,230),RGB(235,175,80)};
/*5x7 glyphs, columns low bit at top; all labels use a complete ASCII subset. */
static const uint8_t letters[26][5]={
 {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {3,4,120,4,3},{97,81,73,69,67}};
static const uint8_t digits[10][5]={
 {62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},
 {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
 {54,73,73,73,54},{6,73,73,41,30}};
static unsigned glyph(char c,unsigned col) {
    if(c>='a'&&c<='z')c-=32;
    if(c>='A'&&c<='Z')return letters[c-'A'][col];
    if(c>='0'&&c<='9')return digits[c-'0'][col];
    switch(c){case '-':return 8;case '+':return col==2?62:8;
    case '.':return col==2?64:0;case ':':return col==2?36:0;
    case '/':return 32>>col;case '[':return col==1?127:col>1?65:0;
    case ']':return col==3?127:col<3?65:0;case '>':return col==1?65:col==2?34:col==3?20:0;
    default:return 0;}
}
static void rect(uint8_t *r,unsigned y,int x,int top,int w,int h,unsigned color) {
    int end=x+w;if((int)y<top||(int)y>=top+h)return;if(x<0)x=0;if(end>240)end=240;
    if(end>x)memset(r+x,(int)color,(size_t)(end-x));
}
static void text(uint8_t *r,unsigned y,int x,int top,const char *s,unsigned scale,unsigned color) {
    unsigned row,i,k;int pos;
    if((int)y<top||y>=top+7*scale)return;row=(y-top)/scale;
    for(i=0;s[i]&&x+(int)(i*6*scale)<240;i++)for(k=0;k<5;k++)if(glyph(s[i],k)&(1u<<row))
        for(pos=0;pos<(int)scale;pos++){int at=x+(int)((i*6+k)*scale)+pos;if(at>=0&&at<240)r[at]=(uint8_t)color;}
}
void fm1_screen_title(fm1_screen_view *v,const uint8_t *data,size_t n) {
    size_t i,j=0;unsigned bracket=0;memset(v->title,0,sizeof(v->title));memset(v->subtitle,0,sizeof(v->subtitle));
    for(i=0;i<n;i++) {
        unsigned c=data[i];if(c=='['){bracket=1;j=0;continue;}if(bracket&&c==']')break;
        if(c<32||c>126)c=' ';if(c==' '&&!j)continue;
        if(!bracket){if(j<32)v->title[j++]=(char)c;}else if(j<38)v->subtitle[j++]=(char)c;
    }
    for(i=strlen(v->title);i&&v->title[i-1]==' ';i--)v->title[i-1]=0;
    for(i=strlen(v->subtitle);i&&v->subtitle[i-1]==' ';i--)v->subtitle[i-1]=0;
    if(!v->title[0])strcpy(v->title,"MDX PLAYER");
}
void fm1_screen_indices(const fm1_screen_view *v,unsigned y,uint8_t r[240]) {
    static const char names[16][3]={"F1","F2","F3","F4","F5","F6","F7","F8","P1","P2","P3","P4","P5","P6","P7","P8"};
    char s[40];unsigned i,seg;
    memset(r,0,240);if(y>=240)return;
    rect(r,y,0,0,240,21,1);rect(r,y,0,20,240,1,2);
    text(r,y,8,5,"FM1",2,8);text(r,y,49,9,"MDX",1,6);
    if(y>=8 && y<15){snprintf(s,sizeof(s),"%02lu:%02lu",(unsigned long)(v->seconds/60),(unsigned long)(v->seconds%60));text(r,y,125,8,s,1,5);}
    text(r,y,188,8,v->running?"PLAY >":"STOP",1,v->running?14:6);
    /* Song labels stay inside the card at native LCD resolution. */
    rect(r,y,4,25,232,32,1);text(r,y,8,28,v->title,1,8);text(r,y,8,43,v->subtitle[0]?v->subtitle:(v->uploaded?"USB SONG / RAM":"FLASH DEMO"),1,6);
    text(r,y,8,64,"STEREO FFT",1,6);text(r,y,164,64,"44.1 KHZ",1,6);
    if(y>=76 && y<111)for(i=0;i<24;i++) {
        int height=(v->spectrum[i]*35+254)/255;
        rect(r,y,8+(int)i*9,76,7,35,3);
        for(seg=0;seg<12;seg++)if((int)seg*3<height)rect(r,y,8+(int)i*9,109-(int)seg*3,7,2,seg<4?10:seg<8?11:14);
    }
    text(r,y,8,112,"172",1,6);text(r,y,88,112,"2K",1,6);text(r,y,189,112,"22K HZ",1,6);
    for(i=0;i<2;i++){text(r,y,8,120+(int)i*8,i?"R":"L",1,6);rect(r,y,20,121+(int)i*8,210,4,3);rect(r,y,20,121+(int)i*8,(int)v->stereo[i]*210/255,4,12);}
    text(r,y,8,140,"FM / MDX",1,6);text(r,y,164,140,"8 PARTS",1,6);
    text(r,y,8,181,"PCM8",1,6);text(r,y,164,181,"8 VOICES",1,6);
    /* All16 voice peaks: FM before shared nonlinear sum; PCM after gain/pan. */
    if((y>=149 && y<178)||(y>=190 && y<219))for(i=y<180?0:8;i<(y<180?8u:16u);i++) {
        int top=i<8?151:192,x=8+(int)(i%8)*28;
        unsigned muted=(v->mutes>>i)&1,selected=i==v->selected;
        rect(r,y,x-2,top-2,26,29,selected?2:1);
        for(seg=0;seg<6;seg++){unsigned color=3;if(v->parts[i]>seg*255/6)color=muted?15:seg<2?10:seg<4?11:14;
            rect(r,y,x+1,top+15-(int)seg*3,21,2,color);}
        if(v->hold[i])rect(r,y,x+1,top+15-(int)((v->hold[i]-1)*6/255)*3,21,1,muted?15:8);
        text(r,y,x+6,top+20,names[i],1,muted?15:selected?8:6);
    }
    if(y>=222 && y<229){snprintf(s,sizeof(s),"FM%u %s  OCT%+d",v->selected+1,(v->mutes&(1u<<v->selected))?"KARAOKE":"MDX",v->octave);text(r,y,8,222,s,1,5);}
    rect(r,y,0,233,240,7,1);text(r,y,3,233,"SELECT PART  FX MUTE  PLAY/STOP",1,6);
}
void fm1_screen_row(const fm1_screen_view *v,unsigned y,uint8_t out[480]) {
    uint8_t r[240];unsigned x;fm1_screen_indices(v,y,r);
    for(x=0;x<240;x++){uint16_t c=fm1_screen_palette[r[x]];out[2*x]=(uint8_t)(c>>8);out[2*x+1]=(uint8_t)c;}
}
static int hex8(const char *s,uint32_t *v) {
    unsigned i;*v=0;
    for(i=0;i<8;i++){unsigned d;char c=s[i];if(c>='0'&&c<='9')d=c-'0';else if(c>='a'&&c<='f')d=c-'a'+10;else if(c>='A'&&c<='F')d=c-'A'+10;else return 0;*v=(*v<<4)|d;}
    return 1;
}
static uint32_t crc(const fm1_screen_view *view,void (*yield)(void *),void *u) {
    uint32_t c=0xffffffffu;unsigned i,j;uint8_t indices[240];
    for(i=0;i<FM1_SCREEN_BYTES;i++){if(i%120==0)fm1_screen_indices(view,i/120,indices);c^=(indices[(i%120)*2]<<4)|indices[(i%120)*2+1];for(j=0;j<8;j++)c=(c>>1)^(0xedb88320u&(0u-(c&1)));if(yield && i%480==479)yield(u);}
    return c^0xffffffffu;
}
int fm1_screen_command(fm1_screen *s,fm1_screen_snapshot snapshot,void (*yield)(void *),void *u,const char *command,uint32_t now,fm1_mdx_reply reply,void *ctx) {
    size_t len=strlen(command),prefix;uint32_t token,offset;char out[240];unsigned i,n,last_y=240;uint8_t indices[240];
    if(strncmp(command,"MDX SHOT",8))return 0;
    if(s->active && (uint32_t)(now-s->last_ms)>30000)s->active=0;
    if(!strcmp(command,"MDX SHOT BEGIN")) {
        s->active=0;
        if(snapshot(u,&s->view,&s->frame)){reply(ctx,"ERR MDX SHOT NO_COMPLETE_FRAME\n");return 1;}
        s->view.title[32]=0;s->view.subtitle[38]=0;
        if(!++s->token)++s->token;s->crc=crc(&s->view,yield,u);s->last_ms=now;s->active=1;
        snprintf(out,sizeof(out),"OK MDX SHOT %08lx 240 240 I4 %08lx frame=%lu\n",(unsigned long)s->token,(unsigned long)s->crc,(unsigned long)s->frame);reply(ctx,out);return 1;
    }
    if(len==31 && !memcmp(command,"MDX SHOT READ ",14) && command[22]==' ' && hex8(command+14,&token) && hex8(command+23,&offset)) {
        if(!s->active || token!=s->token || offset>=FM1_SCREEN_BYTES || offset%FM1_SCREEN_CHUNK)goto bad;
        s->last_ms=now;n=FM1_SCREEN_BYTES-offset;if(n>FM1_SCREEN_CHUNK)n=FM1_SCREEN_CHUNK;
        snprintf(out,sizeof(out),"MDX SHOT DATA %08lx %08lx ",(unsigned long)token,(unsigned long)offset);
        prefix=strlen(out);
        for(i=0;i<n;i++){static const char digits[]="0123456789abcdef";unsigned pos=offset+i,y=pos/120;if(y!=last_y){fm1_screen_indices(&s->view,y,indices);last_y=y;}uint8_t v=(uint8_t)((indices[(pos%120)*2]<<4)|indices[(pos%120)*2+1]);out[prefix+2*i]=digits[v>>4];out[prefix+2*i+1]=digits[v&15];}
        out[prefix+2*n]='\n';out[prefix+2*n+1]=0;reply(ctx,out);return 1;
    }
    if(len==21 && !memcmp(command,"MDX SHOT END ",13) && hex8(command+13,&token)) {
        if(!s->active || token!=s->token)goto bad;s->active=0;reply(ctx,"OK MDX SHOT END\n");return 1;
    }
bad:reply(ctx,"ERR MDX SHOT TOKEN_OFFSET_OR_COMMAND\n");return 1;
}
