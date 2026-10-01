/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FM1_BOOTLAB_STARTUP_H
#define FM1_BOOTLAB_STARTUP_H
#include <stddef.h>
#include <stdint.h>

/* Caller supplies writable storage; zero length performs no access. */
void lab_clear_bytes(volatile uint8_t *begin, size_t bytes);
/* Synthetic validation only: bits 0/1/2 mean valid/corrupt/bounds behaved. */
uint32_t lab_synthetic_check(void);
#define LAB_CHECK_PASSED 7u
#define LAB_FINISHED_MARKER 0x424c4142u
#endif
