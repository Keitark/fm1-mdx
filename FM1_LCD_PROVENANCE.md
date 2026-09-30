# LCD initialization: source comparison

Reviewed 2026-09-30. This is a bounded technical comparison, not a finding about
copyright ownership, permission or infringement. No runtime code was changed.

## Scope and sources

The existing `panel_init[21][18]` arrays in
`firmware/nes/boot/display_test.c` and `firmware/nes/src/fm1_board.c` are identical.
Their comments identify FM-1_010 analysis as their origin. Later corroboration
does not make that development clean-room or reassign its source to the SDK.

The padded 378-byte array SHA-256 is
`280cb746088bea4a2fce2819c1415da0597a286120e3a8229449fbafdc1280c7`.
The comparison reads source, not a stock dump or connected device.

References:

- Jieli-Tech AC79 SDK commit `e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d`:
  [S](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/blob/e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d/apps/common/ui/lcd_driver/lcd_st7789s.c),
  [V](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/blob/e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d/apps/common/ui/lcd_driver/lcd_st7789v.c),
  [T3](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/blob/e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d/apps/common/ui/lcd_driver/lcd_st7789t3.c).
  S/V comparisons inspect `code1`; T3 inspects `LCD_Init`.
- Sitronix [ST7789V specification v1.0, 2013/10](https://dl.espressif.com/dl/schematics/ST7789V_SPEC_V1.0.pdf),
  publicly hosted by Espressif. Section/page references below use its printed
  numbering. This is a controller-family reference, not proof of the FM-1's
  exact controller or module revision.
- Sitronix [ST7789V2 specification v1.0, 2016/11](https://files.waveshare.com/wiki/common/ST7789V2.pdf),
  hosted by Waveshare, cross-checked for C2 and D0 only (pp. 280 and 293).

The PDFs carry rights/confidentiality notices. Public hosting is not permission
to redistribute the documents; neither PDF nor page images are bundled here.
SDK license/attribution boundaries are in [PROVENANCE.md](PROVENANCE.md).

## All 21 records

Payloads below are taken from this repository's existing source and sender
loops. Hexadecimal bytes are shown in wire order; `-` means no payload.
SDK matches are individual literal command/payload candidates, not an assertion
that complete sequences or panel configurations are equivalent. Conditional
source alternatives are retained, not evaluated by a C preprocessor.

Decision codes describe the engineering evidence, **not publication permission**:

- **K**: keep for now; routine documented operation with a usable reference.
- **B**: keep privately pending the publication decision; board calibration or
  geometry still needs an independent panel/board basis if it is to be replaced.
- **Q**: keep privately pending the publication decision; resolve a specific
  controller/protocol discrepancy before proposing a replacement.

| Row | Command / payload from local source | V reference: function, section (page) | Exact SDK candidates: file / line | Decision |
| --- | --- | --- | --- | --- |
| 0 | `11 / -` | Sleep out, 9.1.12 (181) | S:184, V:131, T3:266 | K |
| 1 | Delay 120 ms; not a command | SDK delay examples | S:156, V:130, V:132, T3:267 | K |
| 2 | `2A / 00 00 00 EF` | Columns, 9.1.20 (195) | V:148 | K |
| 3 | `2B / 00 28 01 17` | Rows, 9.1.21 (197) | None | B |
| 4 | `B2 / 0C 0C 0C 00 33` | Porch, 9.2.3 (260) | None | Q |
| 5 | `20 / -` | Inversion off, 9.1.15 (185) | None | K |
| 6 | `B7 / 56` | Gate control, 9.2.5 (263) | None | B |
| 7 | `BB / 18` | VCOM, 9.2.7 (266) | None | B |
| 8 | `C0 / 2C` | LCM control, 9.2.8 (268) | S:172, T3:300 | K |
| 9 | `C2 / 01` | VDV/VRH enable, 9.2.10 (270) | S:173, V:138, T3:303 | Q |
| 10 | `C3 / 1F` | VRH, 9.2.11 (271) | None | B |
| 11 | `C4 / 20` | VDV, 9.2.12 (273) | S:175, V:140, T3:312 | K |
| 12 | `C6 / 0F` | Frame control, 9.2.14 (277) | V:141, T3:315 | K |
| 13 | `D0 / A6 A1` | Power control, 9.2.19 (283) | None | Q |
| 14 | `E0 / D0 0D 14 0B 0B 07 3A 44 50 08 13 13 2D 32` | Positive gamma, 9.2.22 (287) | None | B |
| 15 | `E1 / D0 0D 14 0B 0B 07 3A 44 50 08 13 13 2D 32` | Negative gamma, 9.2.23 (289) | None | B |
| 16 | `36 / 00` | Orientation, 9.1.28 (212) | V:133, T3:280 | K |
| 17 | `3A / 55` | Pixel format, 9.1.32 (221) | S:169, T3:284 | K |
| 18 | `E7 / 00` | SPI2 enable, 9.2.27 (297) | None | K |
| 19 | `51 / FF` | Brightness, 9.1.37 (230) | None | K |
| 20 | `21 / -` | Inversion on, 9.1.16 (187) | S:157, V:147, T3:357 | K |

Ten records match at least one SDK candidate. Eleven do not match these three
initializers; that is not a claim of absence across the SDK or public research.
All 20 command opcodes have controller-documentation references. Those facts do
not independently justify every parameter or the complete FM-1 sequence.

## What the sender actually does

- Row 1 is `{0x45,120}` internally. Both loops special-case its index and wait;
  they do not transmit command 45 or 120 data bytes.
- Row 4 declares a five-byte payload but contains six explicit data bytes.
  Its final `33` is unused. The actual payload is **0C 0C 0C 00 33**, not the
  SDK V example's **0C 0C 00 33 33**. Padding is also not sent.
- Both initialization functions fill a framebuffer and issue `29` separately
  after the table. It is not a missing table row.
- The row window encodes 40 through 279: 240 rows. The SDK V example instead
  uses 0 through 239. Correct module placement remains a board-level question.
- PA2 is driven low in the current working path. This review does not establish
  its electrical role or a backlight-off polarity.

## Three discrepancies requiring investigation, not automatic fixes

1. **B2:** the V reference marks bits set by the third local payload byte as
   fixed zero. Confirm the exact controller and observed transaction first.
2. **C2:** all three SDK examples send one byte, matching this project. V and V2
   instead describe two parameters, `01 FF`. An SDK match alone is not protocol
   qualification; do not append a byte without testing the actual controller.
3. **D0:** this project sends `A6 A1`; all three SDK examples send `A4 A1`.
   V and V2 specify first byte `A4`. Identify the variant before changing it.

These are technical reference discrepancies, not findings of infringement or
proof that the working device is misconfigured. No change to any of these bytes
is proposed in this review. Likewise, replacing voltage/gamma values merely to
match another module could break the display and would not establish clean-room
provenance. The external examples use different B7, BB, C3 and gamma values.

## Reproduce the literal comparison

Use an existing SDK checkout at the exact pin; this command performs no fetch,
build, device access or write:

```powershell
python scripts/audit_lcd_references.py --sdk <existing-sdk-checkout>
python -m unittest discover -s tests -v
```

The script reads pinned Git blobs, not potentially modified working-tree SDK
files, checks both local arrays and prints JSON with match locations and hashes.
It is a bounded literal parser for these sources, not a general C interpreter.
Tests run without an SDK; the real pinned-SDK comparison is a separate local
check. Datasheet interpretation is manual, not verified by the script.

Pinned SDK blob SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `lcd_st7789s.c` | `93cf496e1da4cacdc6b4023a3bdc1794fc8948edeaaa848d22e0a3653b90f351` |
| `lcd_st7789v.c` | `620c082ac5a4a9499aae0f007d0c6533bb0109ddc67d8f763435d15ce287d2f9` |
| `lcd_st7789t3.c` | `20a3a8554e7f90025c5655c6db38f1d266ece2d247d27022816451304c3c902c` |

## Publication recommendation

Keep the truthful origin statement and this evidence map. Do not label the
entire table SDK-derived or silently change a working sequence for cosmetic
provenance reasons. This comparison neither mandates discarding technical
constants nor grants permission to publish the retained implementation.

If the maintainer requires a release without the retained stock-derived table,
prepare a separately reviewed panel profile from suitable documentation/licensed
examples and qualify it on the actual module, retaining an honest development
record. Until that decision, leave the candidate private. The stock boot ABI and
other board findings remain separate items in [PUBLICATION.md](PUBLICATION.md);
replacing this table alone would not settle them.
