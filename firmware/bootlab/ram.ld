/* SPDX-License-Identifier: GPL-3.0-or-later
 * RAM origin/length reference: official fw-Bootloader at
 * ff3b9299d1b94d834b995fd858b3f75e2faa9840,
 * user_boot/cpu/wl82/output/ram_ld.c. All sections/stacks here are our design.
 * This window is a lab assumption, not a verified FM1 ROM reservation map.
 */
ENTRY(_start)
MEMORY { LAB_RAM (rwx) : ORIGIN = 0x01c02000, LENGTH = 128K }
SECTIONS {
    __lab_ram_begin = ORIGIN(LAB_RAM);
    __lab_ram_end = ORIGIN(LAB_RAM) + LENGTH(LAB_RAM);
    .start : { KEEP(*(.start)) } > LAB_RAM
    .text : { *(.text .text.*) } > LAB_RAM
    .rodata ALIGN(32) : { *(.rodata .rodata.*) } > LAB_RAM
    .data ALIGN(32) : { *(.data .data.*) } > LAB_RAM
    .bss ALIGN(32) (NOLOAD) : {
        __lab_bss_begin = .;
        *(.bss .bss.*) *(COMMON)
        . = ALIGN(32);
        __lab_bss_end = .;
    } > LAB_RAM
    .stacks ALIGN(32) (NOLOAD) : {
        __lab_sp_begin = .;
        . += 2048;
        __lab_sp_top = .;
        __lab_ssp_begin = .;
        . += 2048;
        __lab_ssp_top = .;
    } > LAB_RAM
    __lab_used_end = .;
    /DISCARD/ : { *(.comment) *(.note.GNU-stack) }
}
ASSERT(_start == __lab_ram_begin, "entry must start at lab RAM origin")
ASSERT(__lab_bss_end <= __lab_sp_begin, "BSS must not clear stacks")
ASSERT((__lab_sp_top & 31) == 0 && (__lab_ssp_top & 31) == 0, "stack alignment")
ASSERT(__lab_used_end <= __lab_ram_end, "lab RAM overflow")
