/* SPDX-License-Identifier: GPL-3.0-or-later
 * Offline entry design, not an established WL82 ROM-entry contract.
 * Requires code/data already resident in RAM, a quiescent caller and suitable
 * CPU/cache state. Does not disable interrupts or touch hardware registers.
 */
.section .start,"ax",@progbits
.global _start
.type _start,@function
_start:
    r0 = __lab_sp_top
    sp = r0
    r0 = __lab_ssp_top
    ssp = r0
    call lab_entry_c
.Lpark:
    nop
    goto .Lpark
.size _start, .-_start
