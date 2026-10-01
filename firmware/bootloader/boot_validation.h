#ifndef FM1_BOOT_VALIDATION_H
#define FM1_BOOT_VALIDATION_H

#include <stddef.h>
#include <stdint.h>

/* Original portable validation. Values are adapter-supplied, not recovered
 * constants. These structures are in-memory contracts, not disk formats. */
typedef struct {
    uint32_t flash_bytes, sfc_flash_base, xip_base;
    uint32_t app_slot_offset, app_slot_bytes, max_app_bytes;
} fm1_boot_policy;

typedef struct {
    uint32_t flash_offset, bytes, entry;
    uint16_t payload_crc16, version;
    uint32_t features; /* Version 1 supports zero features only. */
} fm1_boot_application;

typedef enum {
    FM1_BOOT_OK = 0, FM1_BOOT_ARGUMENT, FM1_BOOT_LAYOUT,
    FM1_BOOT_UNSUPPORTED, FM1_BOOT_ENTRY, FM1_BOOT_CRC
} fm1_boot_result;

/* Nonzero size requires a readable data pointer. CRC is corruption detection,
 * not authenticity or authorization. Empty input has CRC zero. */
uint16_t fm1_boot_crc16(const uint8_t *data, size_t bytes);
int fm1_boot_layout_valid(const fm1_boot_policy *policy);
fm1_boot_result fm1_boot_validate(const fm1_boot_policy *,
    const fm1_boot_application *, const uint8_t *decoded_payload, size_t bytes);

#endif
