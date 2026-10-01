#include "boot_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

int main(void) {
    /* Entirely synthetic application/metadata; no vendor binary or unit data. */
    const uint8_t payload[]={0x14,0x94,0x80,0x00};
    fm1_boot_policy p={0x100000,0x4000,0x2000000,0x4120,0x70000,0x70000};
    fm1_boot_application a={0x4120,4,0x2000120,0,1,0}, bad;
    fm1_boot_policy q;
    fm1_boot_board_info board={0};
    fm1_boot_handoff h, before;
    uint8_t corrupt[4]; unsigned i;
    a.payload_crc16=fm1_boot_crc16(payload,sizeof(payload));
    CHECK(fm1_boot_crc16((const uint8_t *)"123456789",9)==0x31c3);
    CHECK(fm1_boot_validate(&p,&a,payload,4)==FM1_BOOT_OK);
    memcpy(corrupt,payload,4);corrupt[2]^=1;
    CHECK(fm1_boot_validate(&p,&a,corrupt,4)==FM1_BOOT_CRC);
    CHECK(fm1_boot_validate(&p,&a,payload,3)==FM1_BOOT_LAYOUT);
    CHECK(fm1_boot_validate(NULL,&a,payload,4)==FM1_BOOT_ARGUMENT);
    bad=a;bad.version=2;CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_UNSUPPORTED);
    bad=a;bad.features=1;CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_UNSUPPORTED);
    bad=a;bad.flash_offset=0xfffffff0;CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_LAYOUT);
    bad=a;bad.flash_offset=p.app_slot_offset-1;CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_LAYOUT);
    bad=a;bad.flash_offset=p.app_slot_offset+p.app_slot_bytes-2;
    CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_LAYOUT);
    bad=a;bad.entry++;CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_ENTRY);
    bad=a;bad.entry+=4;CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_ENTRY);
    bad=a;bad.entry-=2;CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_ENTRY);
    bad=a;bad.entry+=2;CHECK(fm1_boot_validate(&p,&bad,payload,4)==FM1_BOOT_OK);
    q=p;q.xip_base=0xfffffff0;CHECK(fm1_boot_validate(&q,&a,payload,4)==FM1_BOOT_LAYOUT);
    q=p;q.app_slot_bytes=UINT32_MAX;CHECK(fm1_boot_validate(&q,&a,payload,4)==FM1_BOOT_LAYOUT);
    q=p;q.max_app_bytes=2;CHECK(fm1_boot_validate(&q,&a,payload,4)==FM1_BOOT_LAYOUT);
    q=p;q.sfc_flash_base=q.flash_bytes;CHECK(fm1_boot_validate(&q,&a,payload,4)==FM1_BOOT_LAYOUT);
    board.chip_id=0x1234;board.trim_value=0x5678;
    for(i=0;i<8;i++)board.bt_mac_record[i]=(uint8_t)(i+1);
    for(i=0;i<32;i++)board.flash_header[i]=(uint8_t)(i+32);
    memset(&h,0xaa,sizeof(h));
    CHECK(fm1_boot_prepare_handoff(&p,&board,0x1c07000,0x1c07000,0x1c07100,&h)==FM1_BOOT_OK);
    CHECK(h.argument[0]==0x5c && h.argument[1]==0x70 && h.argument[2]==0xc0 && h.argument[3]==1);
    CHECK(h.argument[4]==0 && h.argument[5]==0x40);
    CHECK(h.argument[8]==0 && h.argument[9]==0 && h.argument[10]==0 && h.argument[11]==2);
    CHECK(h.argument[12]==0x34 && h.argument[13]==0x12 && h.argument[14]==0x78 && h.argument[15]==0x56);
    CHECK(!memcmp(h.argument+16,board.bt_mac_record,8));
    CHECK(!memcmp(h.flash_header,board.flash_header,32));
    for(i=24;i<92;i++)CHECK(h.argument[i]==0);
    before=h;
    CHECK(fm1_boot_prepare_handoff(&p,&board,0x1c07002,0x1c07000,0x1c07100,&h)==FM1_BOOT_LAYOUT);
    CHECK(!memcmp(&h,&before,sizeof(h)));
    CHECK(fm1_boot_prepare_handoff(&p,&board,0x1c07084,0x1c07000,0x1c07100,&h)==FM1_BOOT_OK);
    before=h;
    CHECK(fm1_boot_prepare_handoff(&p,&board,0x1c07088,0x1c07000,0x1c07100,&h)==FM1_BOOT_LAYOUT);
    CHECK(!memcmp(&h,&before,sizeof(h)));
    CHECK(fm1_boot_prepare_handoff(&p,&board,0xfffffffc,0xffffff00,UINT32_MAX,&h)==FM1_BOOT_LAYOUT);
    puts("PASS source boot validation, corrupt payload, bounds/overflow, entry, complete handoff and transactional errors");
    return 0;
}
