/* Actual synchronous LCD driver at a mocked MMIO boundary. No device I/O. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FM1_DISPLAY_TEST_HOST 1
#include "../../nes/boot/display_test.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"LCD profile failed %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t registers[32],addresses[32],count,source_clock=60000000;
static uint8_t dma_byte;static unsigned dma_size;
volatile uint32_t *fm1_display_test_register(uint32_t a) {
    unsigned i;for(i=0;i<count;i++)if(addresses[i]==a)break;
    if(i==count){CHECK(count<32);addresses[count++]=a;}
    if(a==0x11d00)registers[i]|=0x8000; /* Synchronous transfer completed. */
    return &registers[i];
}
uint32_t timer_get_ms(void){return 0;}
void os_time_dly(int n){(void)n;}
void wdt_clear(void){}
int clk_get(const char *name){CHECK(!strcmp(name,"lsb"));return (int)source_clock;}
int gpio_direction_output(unsigned int p,int v){(void)p;(void)v;return 0;}
int gpio_set_direction(unsigned int p,unsigned int v){(void)p;(void)v;return 0;}
void fm1_display_snapshot(unsigned p,const uint32_t r[20]){(void)p;(void)r;}
void fm1_display_test_dma_source(const uint8_t *p){dma_byte=p[0];dma_size++;}
int main(void) {
    CHECK(fm1_display_mdx_stream_start()==-4);
    enabled=1;BAUD=4;CHECK(!fm1_display_mdx_stream_start());
    CHECK(BAUD==FM1_MDX_LCD_BAUD && BUF==0x3a && dma_byte==0x53 && dma_size==1 && DMA_CNT==1);
    /* Refuse an unexpected LSB clock before changing COLMOD or its divider. */
    source_clock=48000000;BAUD=4;CHECK(fm1_display_mdx_stream_start()==-11);
    CHECK(!enabled && BAUD==4 && dma_size==1);
    puts("PASS synchronous RGB444 COLMOD/divider and unexpected-clock refusal");return 0;
}
