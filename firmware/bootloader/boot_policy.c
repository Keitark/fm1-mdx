#include "boot_policy.h"

_Static_assert(sizeof(fm1_boot_handoff) == 124, "Unexpected handoff padding");

static int range(uint32_t at, uint32_t size, uint32_t begin, uint32_t end) {
    return begin < end && at >= begin && at < end && size && size <= end-at;
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
    if(!fm1_boot_layout_valid(p) || (address&3) ||
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
