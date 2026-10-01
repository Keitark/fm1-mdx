#ifndef FM1_BOOT_POLICY_H
#define FM1_BOOT_POLICY_H
#include <stddef.h>
#include <stdint.h>

/* Original boot policy, not a target driver or an on-flash file format.
 * The flash adapter must provide the decoded application bytes and validated
 * local board metadata. No SDK archive, stock image or device I/O is used here. */
typedef struct {
    uint32_t flash_bytes, sfc_flash_base, xip_base;
    uint32_t app_slot_offset, app_slot_bytes, max_app_bytes;
} fm1_boot_policy;

typedef struct {
    uint32_t flash_offset, bytes, entry;
    uint16_t payload_crc16, version;
    uint32_t features; /* MVP supports zero: no SDRAM/external app. */
} fm1_boot_application;

typedef enum {
    FM1_BOOT_OK = 0, FM1_BOOT_ARGUMENT, FM1_BOOT_LAYOUT,
    FM1_BOOT_UNSUPPORTED, FM1_BOOT_ENTRY, FM1_BOOT_CRC
} fm1_boot_result;

typedef struct {
    uint8_t argument[92];
    uint8_t flash_header[32];
} fm1_boot_handoff;

/* Supplied by the future board adapter; never invent calibration or identity. */
typedef struct {
    uint16_t chip_id, trim_value;
    uint8_t bt_mac_record[8];
    uint8_t flash_header[32];
} fm1_boot_board_info;

uint16_t fm1_boot_crc16(const uint8_t *data, size_t bytes);
fm1_boot_result fm1_boot_validate(const fm1_boot_policy *,
    const fm1_boot_application *, const uint8_t *decoded_payload, size_t bytes);

/* Separate from payload validation. Caller must validate immediately before
 * use and prevent modifications until transfer of control. This prepares bytes
 * only; it does not establish flash mapping, stacks, or a bootable image.
 * handoff_address is the target RAM address of the output, not its host pointer.
 * ram_begin/end must describe usable RAM after subtracting ROM/IRQ/stack and
 * loader allocations. Errors leave the output unchanged. */
fm1_boot_result fm1_boot_prepare_handoff(const fm1_boot_policy *,
    const fm1_boot_board_info *, uint32_t handoff_address,
    uint32_t ram_begin, uint32_t ram_end, fm1_boot_handoff *);
#endif
