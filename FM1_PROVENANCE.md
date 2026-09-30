# Source references and research boundaries

This is a focused engineering inventory, not a clean-room claim, an exhaustive
authorship audit or a legal opinion. It preserves the known development history.
Public documentation found later can corroborate a fact without changing where
the existing implementation originally came from. No firmware behavior was
changed for this publication-preparation review.

## Pinned dependencies

- Jieli-Tech AC79 SDK: `e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d`.
  [Repository](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK), repository-level
  Apache-2.0. Individual files and linked archives may have additional terms.
- PeakRacing NES: `68bdfc8de570264c0e84f73766f1a1ed3591066f`.
  [Repository](https://github.com/PeakRacing/nes), Apache-2.0; retain upstream
  copyright and license notices.

## Focused source map

Paths below are relative to this repository. SDK paths refer to the pinned
external checkout, not bundled files. These are evidence categories, not
automatic permission or infringement determinations.

| Local component | Basis and corresponding reference | What remains FM-1-specific or unreviewed |
| --- | --- | --- |
| `firmware/nes/boot/board_power.c`, `board_power.h`, `board.c` | Calls the SDK `power_init` API using `struct low_power_param`; declarations/selectors are in `include_lib/driver/cpu/wl82/asm/power_interface.h` and `p33.h`. Official system docs describe these fields. | The selected voltage levels and configuration were recovered from FM-1_010. SDK field definitions do not independently establish the right settings for this PCB. |
| `firmware/nes/boot/boot_compat.c` | Project adapter calls the existing SDK `boot_info_init`, copying six input words and extending the argument to 23 words. | The stock bootloader handoff contract comes from stock analysis. Official UBOOT selection documentation does not establish this exact ABI. |
| `firmware/nes/boot/display_test.c`, `firmware/nes/src/fm1_board.c` | GPIO/register APIs are available in SDK `asm/gpio.h`, `asm/WL82.h`; SDK `apps/common/ui/lcd_driver/lcd_st7789s.c`, `lcd_st7789v.c`, `lcd_st7789t3.c` provide controller examples. | Both `panel_init` arrays explicitly retain the same 21 x 18-byte FM-1_010 table. SDK ST7789S initialization values inspected in this review differ; the table has NOT been reclassified as SDK-origin. PA2 sequencing, wiring and panel settings remain board-specific. |
| `firmware/nes/boot/pre_os_display.c`, `boot_trace.c`, `wl82_services.c` | Project startup diagnostics and adapters around SDK OS, interrupt, GPIO and audio services. Official system/peripheral docs provide API context. | Initialization ordering and board adaptation need their own evidence; API documentation alone does not establish complete implementation provenance. |
| `firmware/nes/src/fm1_stock_keys.c`, `fm1_wl82_keyscan.c`, `fm1_volume.c` | Project scanning/decoding and ADC handling informed by stock behavior and device tests; SDK SPI/GPIO/ADC documentation describes the interfaces. | Physical slot mapping, encoder protocol and volume input routing are device-specific findings, not generic SDK facts. |
| `firmware/usb-diag/vendor_overlay.py` | Contains matching/replacement fragments adapting pinned SDK `apps/common/usb/device/{cdc.c,usb_device.c,msd_upgrade.c}` and PeakRacing `src/{nes.c,nes_apu.c}`. | These are upstream adaptations, not wholly independent source. Generated copies preserve headers and identify project modifications. |
| `firmware/usb-diag/descriptors.c` | Descriptor code uses the inherited SDK USB identity `3654:5155`, with project diagnostic strings. | No project VID/PID allocation or permission for product distribution has been established by this review. |
| `firmware/nes/audit_{boot,power,pre_os}.py` and related host tests | Project regression checks for reviewed layout, parameters and generated instructions. `audit_power.py` hashes SDK-generated power code, rather than shipping that function as stock machine code. | Audit hashes and test success are technical checks, not evidence of redistribution permission. |
| `firmware/nes/tests/make_diagnostic_rom.py` | Project diagnostic generator for original checkerboard/controller/pulse tests; no commercial ROM input. | A user-supplied ROM remains outside this source release and requires its own rights assessment. |
| Other project adapters, effects, tests, headers, build/release scripts and documentation | Maintained as project contributions; dependencies are separately identified above. | This focused review does not certify exhaustive independent authorship of every remaining line. New borrowed material needs explicit origin/license review. |

The complete 21-record comparison is in [LCD_PROVENANCE.md](LCD_PROVENANCE.md).
Ten records match candidates in the pinned SDK S/V/T3 initializers, while three
protocol/reference discrepancies need further technical review. The report
preserves the table's known origin; it does not approve or change runtime code.

## Official documentation

The pinned SDK README links to the release-v1.2.0 documentation. These references
explain interfaces; they are not FM-1 schematics or blanket redistribution grants.

- [AC79 documentation](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/index.html).
- [System documentation](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/module_example/system/index.html): initialization, memory, OS, interrupts, clocks, voltage configuration (7.42), and UBOOT selection (7.44).
- [Peripheral documentation](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/module_example/peripherals/index.html): GPIO, SPI, ADC, keys, USB and display interfaces.

For PA2, the current working sequence calls `gpio_direction_output(0x02, 0)`.
That records an output level, not proof that PA2 is a dedicated backlight,
reset or output-enable pin. Do not invent a shutdown polarity from its name.

## Linked binary inputs (not distributed here)

The firmware build uses these archives from the SDK:

- `include_lib/newlib/pi32v2-lib/{libm.a,libc.a,libcompiler_rt.a}`
- `cpu/wl82/liba/{cpu.a,event.a,system.a,cfg_tool.a,fs.a,common_lib.a,update.a}`

The compiler/toolchain is also external. Root SDK licensing is not a completed
per-archive redistribution review. Before publishing linked binaries, identify
the applicable terms and notices for each input and for any embedded assets.

## Review outcome

The remaining bounded review is recorded in
[PUBLICATION_AUDIT.md](PUBLICATION_AUDIT.md), including source licenses, the
boot/power comparison, SDK-derived instruction templates in host audit scripts,
and a point-in-time GitHub surface inventory. Source-only does not mean that
every included byte sequence originated independently of the SDK.

The SDK supports understanding and implementing much of the chip-facing code.
It does not, on its own, clear the retained LCD table, establish the stock ABI,
or verify the FM-1 wiring. No ownership determination is inferred from matching
numeric values; a provenance flag is not a finding of infringement. See
PUBLICATION.md for the remaining publication decisions.
