#include "boot_validation.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #condition); \
        exit(1); \
    } \
} while (0)

int main(void) {
    const uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    const uint8_t corrupt[] = {0x11, 0x22, 0x33, 0x45};
    fm1_boot_policy p = {4096, 256, 0x10000, 512, 512, 512}, q;
    fm1_boot_application a = {512, 4, 0x10100, 0, 1, 0}, bad;
    unsigned offset;
    CHECK(fm1_boot_crc16((const uint8_t *)"123456789", 9) == 0x31c3);
    CHECK(fm1_boot_crc16(NULL, 0) == 0);
    CHECK(!fm1_boot_layout_valid(NULL));
    a.payload_crc16 = fm1_boot_crc16(payload, sizeof(payload));
    CHECK(fm1_boot_validate(&p, &a, payload, 4) == FM1_BOOT_OK);
    CHECK(fm1_boot_validate(&p, &a, corrupt, 4) == FM1_BOOT_CRC);
    CHECK(fm1_boot_validate(NULL, &a, payload, 4) == FM1_BOOT_ARGUMENT);
    CHECK(fm1_boot_validate(&p, NULL, payload, 4) == FM1_BOOT_ARGUMENT);
    CHECK(fm1_boot_validate(&p, &a, NULL, 4) == FM1_BOOT_ARGUMENT);
    CHECK(fm1_boot_validate(&p, &a, payload, 3) == FM1_BOOT_LAYOUT);
    bad = a; bad.version = 2;
    CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_UNSUPPORTED);
    bad = a; bad.features = 1;
    CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_UNSUPPORTED);
    bad = a; bad.bytes = 0;
    CHECK(fm1_boot_validate(&p, &bad, payload, 0) == FM1_BOOT_LAYOUT);
    bad = a; bad.bytes = 1;
    CHECK(fm1_boot_validate(&p, &bad, payload, 1) == FM1_BOOT_LAYOUT);

    /* Independently stated interval contract over every fixture-flash offset. */
    for (offset = 0; offset < p.flash_bytes; offset++) {
        bad = a; bad.flash_offset = offset;
        bad.entry = p.xip_base + offset - p.sfc_flash_base;
        if (offset < 512 || offset > 1020) {
            CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_LAYOUT);
        } else if (offset & 1) {
            CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_ENTRY);
        } else {
            CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_OK);
        }
    }
    bad = a; bad.entry += 2;
    CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_OK);
    bad.entry += 2;
    CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_ENTRY);
    bad = a; bad.entry -= 2;
    CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_ENTRY);
    bad = a; bad.flash_offset = UINT32_MAX - 1;
    CHECK(fm1_boot_validate(&p, &bad, payload, 4) == FM1_BOOT_LAYOUT);
    q = p; q.app_slot_bytes = UINT32_MAX;
    CHECK(fm1_boot_validate(&q, &a, payload, 4) == FM1_BOOT_LAYOUT);
    q = p; q.xip_base = UINT32_MAX - 3;
    CHECK(fm1_boot_validate(&q, &a, payload, 4) == FM1_BOOT_LAYOUT);
    q = p; q.xip_base++;
    CHECK(fm1_boot_validate(&q, &a, payload, 4) == FM1_BOOT_LAYOUT);
    q = p; q.sfc_flash_base = q.flash_bytes;
    CHECK(fm1_boot_validate(&q, &a, payload, 4) == FM1_BOOT_LAYOUT);
    q = p; q.max_app_bytes = 2;
    CHECK(fm1_boot_validate(&q, &a, payload, 4) == FM1_BOOT_LAYOUT);
    puts("PASS CRC, corrupt input, null/size/profile rejection, 4096 offsets, entry and overflow boundaries");
    return 0;
}
