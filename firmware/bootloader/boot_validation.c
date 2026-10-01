#include "boot_validation.h"

static int range(uint32_t at, uint32_t size, uint32_t begin, uint32_t end) {
    return begin < end && at >= begin && at < end && size && size <= end - at;
}

int fm1_boot_layout_valid(const fm1_boot_policy *p) {
    uint32_t mapped_bytes;
    if (!p || !p->max_app_bytes || (p->xip_base & 3) ||
        p->sfc_flash_base >= p->flash_bytes ||
        !range(p->app_slot_offset, p->app_slot_bytes, 0, p->flash_bytes) ||
        p->app_slot_offset < p->sfc_flash_base) {
        return 0;
    }
    mapped_bytes = p->flash_bytes - p->sfc_flash_base;
    return mapped_bytes <= UINT32_MAX - p->xip_base;
}

uint16_t fm1_boot_crc16(const uint8_t *data, size_t bytes) {
    uint16_t crc = 0;
    size_t i;
    unsigned bit;
    for (i = 0; i < bytes; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8; bit++) {
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
        }
    }
    return crc;
}

fm1_boot_result fm1_boot_validate(const fm1_boot_policy *p,
    const fm1_boot_application *a, const uint8_t *payload, size_t bytes) {
    uint32_t mapped_start;
    if (!p || !a || !payload) {
        return FM1_BOOT_ARGUMENT;
    }
    if (!fm1_boot_layout_valid(p)) {
        return FM1_BOOT_LAYOUT;
    }
    if (a->version != 1 || a->features) {
        return FM1_BOOT_UNSUPPORTED;
    }
    if (bytes != a->bytes || a->bytes < 2 || a->bytes > p->max_app_bytes ||
        !range(a->flash_offset, a->bytes, p->app_slot_offset,
               p->app_slot_offset + p->app_slot_bytes)) {
        return FM1_BOOT_LAYOUT;
    }
    mapped_start = p->xip_base + (a->flash_offset - p->sfc_flash_base);
    if ((a->entry & 1) || a->entry < mapped_start ||
        a->entry - mapped_start > a->bytes - 2) {
        return FM1_BOOT_ENTRY;
    }
    if (fm1_boot_crc16(payload, bytes) != a->payload_crc16) {
        return FM1_BOOT_CRC;
    }
    return FM1_BOOT_OK;
}
