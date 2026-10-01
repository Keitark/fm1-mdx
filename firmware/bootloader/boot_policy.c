#include "boot_policy.h"

_Static_assert(sizeof(fm1_boot_handoff) == 124, "Unexpected handoff padding");

static int range(uint32_t at, uint32_t size, uint32_t begin, uint32_t end) {
    return begin < end && at >= begin && at < end && size && size <= end-at;
}

static int layout(const fm1_boot_policy *p) {
    uint32_t mapped_bytes;
    if(!p || !p->max_app_bytes || (p->xip_base & 3) ||
       p->sfc_flash_base >= p->flash_bytes ||
       !range(p->app_slot_offset,p->app_slot_bytes,0,p->flash_bytes) ||
       p->app_slot_offset < p->sfc_flash_base)return 0;
    mapped_bytes=p->flash_bytes-p->sfc_flash_base;
    return mapped_bytes <= UINT32_MAX-p->xip_base;
}

uint16_t fm1_boot_crc16(const uint8_t *data, size_t bytes) {
    uint16_t crc=0;
    size_t i; unsigned bit;
    for(i=0;i<bytes;i++) {
        crc^=(uint16_t)data[i]<<8;
        for(bit=0;bit<8;bit++)
            crc=(uint16_t)((crc<<1)^((crc&0x8000)?0x1021:0));
    }
    return crc;
}

fm1_boot_result fm1_boot_validate(const fm1_boot_policy *p,
    const fm1_boot_application *a, const uint8_t *payload, size_t bytes) {
    uint32_t mapped_start;
    if(!p || !a || !payload)return FM1_BOOT_ARGUMENT;
    if(!layout(p))return FM1_BOOT_LAYOUT;
    if(a->version!=1 || a->features)return FM1_BOOT_UNSUPPORTED;
    if(bytes!=a->bytes || a->bytes<2 || a->bytes>p->max_app_bytes ||
       !range(a->flash_offset,a->bytes,p->app_slot_offset,
              p->app_slot_offset+p->app_slot_bytes))return FM1_BOOT_LAYOUT;
    mapped_start=p->xip_base+(a->flash_offset-p->sfc_flash_base);
    if((a->entry&1) || a->entry<mapped_start ||
       a->entry-mapped_start>a->bytes-2)return FM1_BOOT_ENTRY;
    if(fm1_boot_crc16(payload,bytes)!=a->payload_crc16)return FM1_BOOT_CRC;
    return FM1_BOOT_OK;
}

static void word(volatile uint8_t *p, uint32_t x) {
    unsigned i; for(i=0;i<4;i++)p[i]=(uint8_t)(x>>(i*8));
}

fm1_boot_result fm1_boot_prepare_handoff(const fm1_boot_policy *p,
    const fm1_boot_board_info *board, uint32_t address,
    uint32_t ram_begin, uint32_t ram_end, fm1_boot_handoff *out) {
    volatile fm1_boot_handoff result;
    unsigned i;
    if(!p || !board || !out)return FM1_BOOT_ARGUMENT;
    if(!layout(p) || (address&3) ||
       !range(address,sizeof(result),ram_begin,ram_end))return FM1_BOOT_LAYOUT;
    for(i=0;i<sizeof(result.argument);i++)result.argument[i]=0;
    for(i=0;i<sizeof(result.flash_header);i++)
        result.flash_header[i]=board->flash_header[i];
    /* SDK BOOT_DEVICE_INFO common prefix: pointer, SFC mapping, chip/trim,
     * eight-byte MAC record. Put the header after all 92 argument bytes so
     * extensions +80/+88 remain explicitly zero, unlike a +24 header layout. */
    word(result.argument,address+sizeof(result.argument));
    word(result.argument+4,p->sfc_flash_base);
    word(result.argument+8,p->xip_base);
    result.argument[12]=(uint8_t)board->chip_id;
    result.argument[13]=(uint8_t)(board->chip_id>>8);
    result.argument[14]=(uint8_t)board->trim_value;
    result.argument[15]=(uint8_t)(board->trim_value>>8);
    for(i=0;i<8;i++)result.argument[16+i]=board->bt_mac_record[i];
    /* Volatile accesses keep pre-runtime code from acquiring hidden libc
     * memcpy/memset calls through compiler loop/structure transformations. */
    for(i=0;i<sizeof(result);i++)
        ((uint8_t *)out)[i]=((const volatile uint8_t *)&result)[i];
    return FM1_BOOT_OK;
}
