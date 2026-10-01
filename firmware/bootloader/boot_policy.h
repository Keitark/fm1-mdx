#ifndef FM1_BOOT_POLICY_H
#define FM1_BOOT_POLICY_H
#include "boot_validation.h"

/* Original boot policy, not a target driver or an on-flash file format.
 * The flash adapter must provide the decoded application bytes and validated
 * local board metadata. No SDK archive, stock image or device I/O is used here. */
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
