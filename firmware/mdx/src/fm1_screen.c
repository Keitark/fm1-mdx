#include "fm1_screen.h"
#include <stdio.h>
#include <string.h>
#define RGB(r,g,b) (((r)>>3)<<11|((g)>>2)<<5|((b)>>3))
/* Pocket-inspired violet palette. Client expands these exact565 values. */
const uint16_t fm1_screen_palette[16]={
    RGB(4,4,16),RGB(10,10,30),RGB(55,55,100),RGB(20,20,45),
    RGB(30,30,70),RGB(210,210,235),RGB(150,150,180),RGB(210,210,245),
    RGB(245,245,255),RGB(45,30,110),RGB(70,70,140),RGB(110,110,180),
    RGB(150,150,210),RGB(82,82,173),RGB(150,170,230),RGB(235,175,80)};
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
    case '(':return col==1?28:col==2?34:col==3?65:0;
    case ')':return col==1?65:col==2?34:col==3?28:0;
    case '&':{static const uint8_t g[5]={54,73,85,34,80};return g[col];}
    case '?':{static const uint8_t g[5]={2,1,81,9,6};return g[col];}
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
enum {PLOT_HEIGHT=49,PLOT_STEPS=28,SPECTRUM_TOP=79,PART_TOP=164};
static unsigned steps(uint8_t level){return (level*PLOT_STEPS+254u)/255u;}
static unsigned plot_color(uint8_t level,uint8_t cap,unsigned d,unsigned muted) {
    unsigned amount=steps(level),marker=steps(cap),height=(marker*PLOT_HEIGHT+PLOT_STEPS-1)/PLOT_STEPS;
    unsigned unit=d*PLOT_STEPS/PLOT_HEIGHT,color=3;
    if(unit<amount)color=muted?15:unit<9?(d&1?9:10):unit<19?(d&1?10:11):(d&1?11:14);
    /* MMDSP text palette1: RGB5(10,10,21), a blue-violet marker. */
    if(height && d==height-1)color=muted?15:13;
    return color;
}
static void label_copy(char *out,size_t capacity,const char *start,size_t n) {
    while(n && *start==' '){start++;n--;}
    while(n && (start[n-1]==' ' || start[n-1]=='/' || start[n-1]=='-'))n--;
    if(n>=capacity)n=capacity-1;memcpy(out,start,n);out[n]=0;
}
static char lower(char c){return c>='A'&&c<='Z'?c+32:c;}
static const char *credit_start(const char *s) {
    static const char *tokens[]={"reprogrammed by","programmed by","arranged by","composed by","ar.by","by ","by:"};
    size_t i,k,j;
    for(i=0;s[i];i++) {
        if(i && s[i-1]!=' ' && s[i-1]!='/')continue;
        for(k=0;k<sizeof(tokens)/sizeof(tokens[0]);k++) {
            for(j=0;tokens[k][j] && s[i+j] && lower(s[i+j])==tokens[k][j];j++);
            if(!tokens[k][j])return s+i;
        }
    }
    return 0;
}
void fm1_screen_title(fm1_screen_view *v,const uint8_t *data,size_t n) {
    char clean[256];size_t i;const char *bracket,*end,*credit;
    if(n>sizeof(clean)-1)n=sizeof(clean)-1;
    for(i=0;i<n;i++)clean[i]=data[i]>=32 && data[i]<=126?(char)data[i]:data[i]>=128?'?':' ';
    clean[n]=0;bracket=strchr(clean,'[');end=bracket?strchr(bracket+1,']'):0;
    memset(v->title,0,sizeof(v->title));memset(v->subtitle,0,sizeof(v->subtitle));
    memset(v->credit,0,sizeof(v->credit));v->credit_scroll=0;
    credit=credit_start(end?end+1:clean);
    label_copy(v->title,sizeof(v->title),clean,(size_t)((bracket?bracket:credit?credit:clean+n)-clean));
    if(bracket)label_copy(v->subtitle,sizeof(v->subtitle),bracket+1,(size_t)((end?end:clean+n)-(bracket+1)));
    if(!credit && end)credit=end+1;
    if(credit)label_copy(v->credit,sizeof(v->credit),credit,strlen(credit));
    if(!v->title[0])strcpy(v->title,"MDX PLAYER");
    if(!v->credit[0])strcpy(v->credit,"NO EMBEDDED CREDIT");
}
void fm1_screen_indices(const fm1_screen_view *v,unsigned y,uint8_t r[240]) {
    static const char names[16][3]={"F1","F2","F3","F4","F5","F6","F7","F8","P1","P2","P3","P4","P5","P6","P7","P8"};
    char s[40];unsigned i;
    memset(r,0,240);if(y>=240)return;
    rect(r,y,0,0,240,21,1);rect(r,y,0,20,240,1,2);
    text(r,y,8,5,"FM1",2,8);
    if(y>=9 && y<16){snprintf(s,sizeof(s),"OCT%+d",v->octave);text(r,y,49,9,s,1,6);}
    if(y>=8 && y<15){snprintf(s,sizeof(s),"%02lu:%02lu",(unsigned long)(v->seconds/60),(unsigned long)(v->seconds%60));text(r,y,125,8,s,1,5);}
    text(r,y,188,8,v->running?"PLAY >":"STOP",1,v->running?14:6);
    /* Song labels stay inside the card at native LCD resolution. */
    rect(r,y,4,25,232,36,1);text(r,y,8,28,v->title,1,8);text(r,y,8,40,v->subtitle[0]?v->subtitle:(v->uploaded?"USB SONG / RAM":"FLASH DEMO"),1,6);
    if(y>=52 && y<59){text(r,y,8-(int)v->credit_scroll,52,v->credit,1,14);rect(r,y,0,52,8,7,1);rect(r,y,232,52,8,7,1);}
    text(r,y,8,64,"NOTE SPECTRUM",1,6);text(r,y,188,64,"MAX",1,6);
    if(y>=SPECTRUM_TOP && y<SPECTRUM_TOP+PLOT_HEIGHT)for(i=0;i<32;i++) {
        unsigned d=SPECTRUM_TOP+PLOT_HEIGHT-1-y;
        rect(r,y,9+(int)i*7,(int)y,6,1,plot_color(v->spectrum[i],v->spectrum_hold[i],d,0));
    }
    text(r,y,8,130,"28",1,6);text(r,y,88,130,"220",1,6);text(r,y,183,130,"3.5K HZ",1,6);
    text(r,y,8,143,"FM8 + PCM8",1,6);text(r,y,146,143,"VELOCITY",1,6);
    /* MMDSP's two256x56 plots scaled uniformly to224x49, preserving28
       logical vertical steps. All16 parts share the same baseline. */
    if(y>=151 && y<215)for(i=0;i<16;i++) {
        int x=8+(int)i*14;
        unsigned muted=(v->mutes>>i)&1,selected=i==v->selected;
        rect(r,y,x,151,14,64,selected?2:1);
        if(y>=PART_TOP && y<PART_TOP+PLOT_HEIGHT)
            rect(r,y,x+1,(int)y,12,1,plot_color(v->parts[i],v->hold[i],PART_TOP+PLOT_HEIGHT-1-y,muted));
        text(r,y,x+1,153,names[i],1,muted?15:selected?8:6);
    }
    if(y>=222 && y<229){snprintf(s,sizeof(s),"FM%u %s  OCT%+d",v->selected+1,(v->mutes&(1u<<v->selected))?"KARAOKE":"MDX",v->octave);text(r,y,8,222,s,1,5);}
    rect(r,y,0,233,240,7,1);text(r,y,3,233,"SELECT PART  FX MUTE  PLAY/STOP",1,6);
}
void fm1_screen_row(const fm1_screen_view *v,unsigned y,uint8_t out[480]) {
    uint8_t r[240];unsigned x;fm1_screen_indices(v,y,r);
    for(x=0;x<240;x++){uint16_t c=fm1_screen_palette[r[x]];out[2*x]=(uint8_t)(c>>8);out[2*x+1]=(uint8_t)c;}
}
int fm1_screen_row_changed(const fm1_screen_view *a,const fm1_screen_view *b,unsigned y) {
    /* Conservative field dependencies: a false result guarantees identical
       pixels. No second rasterization, hash collisions or framebuffer needed. */
    if(y>=240)return 0;
    if(y>=8 && y<16)return a->seconds!=b->seconds || a->running!=b->running || a->octave!=b->octave;
    if(y>=28 && y<35)return memcmp(a->title,b->title,sizeof(a->title))!=0;
    if(y>=40 && y<47)return a->uploaded!=b->uploaded || memcmp(a->subtitle,b->subtitle,sizeof(a->subtitle))!=0;
    if(y>=52 && y<59)return a->credit_scroll!=b->credit_scroll || memcmp(a->credit,b->credit,sizeof(a->credit))!=0;
    if(y>=SPECTRUM_TOP && y<SPECTRUM_TOP+PLOT_HEIGHT) {
        unsigned d=SPECTRUM_TOP+PLOT_HEIGHT-1-y,i;
        for(i=0;i<32;i++)if(plot_color(a->spectrum[i],a->spectrum_hold[i],d,0)!=plot_color(b->spectrum[i],b->spectrum_hold[i],d,0))return 1;
        return 0;
    }
    if(y>=151 && y<215) {
        unsigned i;
        if(a->selected!=b->selected)return 1;
        for(i=0;i<16;i++) {
            unsigned ma=(a->mutes>>i)&1,mb=(b->mutes>>i)&1;
            if(y>=153 && y<160 && ma!=mb)return 1;
            if(y>=PART_TOP && y<PART_TOP+PLOT_HEIGHT &&
               plot_color(a->parts[i],a->hold[i],PART_TOP+PLOT_HEIGHT-1-y,ma)!=plot_color(b->parts[i],b->hold[i],PART_TOP+PLOT_HEIGHT-1-y,mb))return 1;
        }
        return 0;
    }
    if(y>=222 && y<229)return a->selected!=b->selected || a->mutes!=b->mutes || a->octave!=b->octave;
    return 0;
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
        s->view.credit[127]=0;
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
