#include "boot_validation.h"
#include <stdio.h>

int main(void) {
    /* Arbitrary fixture addresses and data. These are not FM1 flash settings,
     * executable instructions, a stock payload or a public update package. */
    const uint8_t good[] = {0x11, 0x22, 0x33, 0x44};
    const uint8_t corrupt[] = {0x11, 0x22, 0x33, 0x45};
    fm1_boot_policy policy = {4096, 256, 0x10000, 512, 512, 512};
    fm1_boot_application app = {512, 4, 0x10100, 0, 1, 0};
    fm1_boot_result result;
    app.payload_crc16 = fm1_boot_crc16(good, sizeof(good));

    result = fm1_boot_validate(&policy, &app, good, sizeof(good));
    printf("valid: %s (validation only; no jump)\n",
           result == FM1_BOOT_OK ? "ELIGIBLE" : "REJECT");
    if (result != FM1_BOOT_OK) return 1;

    result = fm1_boot_validate(&policy, &app, corrupt, sizeof(corrupt));
    printf("corrupt: REJECT code=%u (no hardware recovery invoked)\n", (unsigned)result);
    if (result != FM1_BOOT_CRC) return 1;

    app.flash_offset = 1024;
    result = fm1_boot_validate(&policy, &app, good, sizeof(good));
    printf("outside allocation: REJECT code=%u\n", (unsigned)result);
    return result != FM1_BOOT_LAYOUT;
}
