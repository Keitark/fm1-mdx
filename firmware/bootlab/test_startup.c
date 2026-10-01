/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "startup.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #c); exit(1); \
} } while (0)

int main(void) {
    uint8_t ram[160];
    size_t length, i;
    /* Poison a simulated BSS and both adjacent regions. Every byte outside
     * the requested interval (including simulated stack space) must survive. */
    for (length = 0; length <= 128; length++) {
        for (i = 0; i < sizeof(ram); i++) ram[i] = 0xa5;
        lab_clear_bytes(ram + 16, length);
        for (i = 0; i < sizeof(ram); i++)
            CHECK(ram[i] == (i >= 16 && i < 16 + length ? 0 : 0xa5));
    }
    lab_clear_bytes(NULL, 0);
    CHECK(lab_synthetic_check() == LAB_CHECK_PASSED);
    puts("startup: 129 BSS spans preserve guards; synthetic checks pass");
    return 0;
}
