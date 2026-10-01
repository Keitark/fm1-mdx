/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "startup.h"

extern uint8_t __lab_bss_begin[], __lab_bss_end[];
/* RAM breadcrumbs for future inspection; not MMIO or a public ROM ABI. */
volatile uint32_t lab_result;
volatile uint32_t lab_finished;

void lab_entry_c(void) {
    lab_clear_bytes(__lab_bss_begin,
                    (size_t)((uintptr_t)__lab_bss_end - (uintptr_t)__lab_bss_begin));
    lab_result = lab_synthetic_check();
    lab_finished = LAB_FINISHED_MARKER;
    /* Return to the assembly park loop. There is no application jump. */
}
