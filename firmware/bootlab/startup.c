/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "startup.h"
#include "boot_validation.h"

void lab_clear_bytes(volatile uint8_t *begin, size_t bytes) {
    /* Volatile stores avoid turning this pre-runtime loop into a libc call. */
    while (bytes--) *begin++ = 0;
}

uint32_t lab_synthetic_check(void) {
    const uint8_t good[] = {0x11, 0x22, 0x33, 0x44};
    const uint8_t corrupt[] = {0x11, 0x22, 0x33, 0x45};
    static const fm1_boot_policy policy = {4096, 256, 0x10000, 512, 512, 512};
    fm1_boot_application app;
    uint32_t result = 0;
    /* Scalar assignments keep startup independent of compiler-emitted memcpy. */
    app.flash_offset = 512;
    app.bytes = 4;
    app.entry = 0x10100;
    app.version = 1;
    app.features = 0;
    app.payload_crc16 = fm1_boot_crc16(good, sizeof(good));
    if (fm1_boot_validate(&policy, &app, good, sizeof(good)) == FM1_BOOT_OK)
        result |= 1;
    if (fm1_boot_validate(&policy, &app, corrupt, sizeof(corrupt)) == FM1_BOOT_CRC)
        result |= 2;
    app.flash_offset = 1024;
    if (fm1_boot_validate(&policy, &app, good, sizeof(good)) == FM1_BOOT_LAYOUT)
        result |= 4;
    return result;
}
