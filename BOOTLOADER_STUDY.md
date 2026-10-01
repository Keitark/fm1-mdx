# Offline WL82 bootloader build and comparison

2026-10-01, Refs #15. **Source compilation succeeds; a rebuilt bootloader is
not available.** No device was queried, reset, flashed, or otherwise operated.
The existing MDX firmware candidate and preserved device rollback are separate.

## Inputs and build result

Jieli's [official user-boot project](https://github.com/Jieli-Tech/fw-Bootloader)
maps AC791N to WL82. This study pins that project at
`ff3b9299d1b94d834b995fd858b3f75e2faa9840` and the AC79 application SDK at
`e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d`.

The available Jieli pi32v2/r3 compiler successfully preprocesses the WL82 linker
script and compiles all **11 upstream C sources** using the upstream flags.
The link attempt fails with:

```text
cannot find .../user_boot/include_lib/liba/wl82/uboot.a: No such file or directory
```

The WL82 `maskrom_stubs.ld` file is also **empty** in this source revision,
whereas the other eight families have populated export tables. The missing
archive is the proven link blocker; whether additional ROM exports are required
cannot be established without that archive and a complete link attempt.
The archive would supply essential startup, architecture/clock, filesystem,
application handoff, and upgrade implementations; declarations alone cannot
replace it. Other chip families' archives are not WL82 replacements. Completion
requires the matching official WL82 library and review of its ROM-symbol
requirements, followed by a fresh linked-code review. This study does not invent
those missing implementations or symbol addresses.

There is no linked `uboot.elf`, freshly built `uboot.boot`, full-device image,
or flash proposal. Vendor post-build scripts are not invoked. A successful
study-script exit means the study finished; inspect `status` and
`link_returncode` in its report for the build outcome.

## Binary comparison that was possible

The pinned AC79 SDK already ships `cpu/wl82/tools/uboot.boot`. This is a
**prebuilt reference**, not the output of the user-boot compilation above.
Its correspondence to the newer customizable user-boot source is not established.
Both the saved FM1 normal boot code and this SDK reference are hash-pinned before
fixed instruction addresses are interpreted. The SDK bank's header and payload
CRC16 checks pass. FM1 code comes from the existing private, CRC-validated normal
boot-bank analysis; no configuration, unit identity, or chip-key material is
read by this study script.

| Reviewed property | FM1 stock normal loader | SDK-shipped prebuilt loader |
|---|---|---|
| Decoded code size | 14,368 bytes | 14,472 bytes (+104) |
| RAM load address | `0x01C02000` | `0x01C02000` |
| Initial BSS range | `0x01C05840..0x01C065A0` | `0x01C058B0..0x01C06320` |
| Initial SSP / SP | `0x01C05C40` / `0x01C06040` | `0x01C05CB0` / `0x01C060B0` |
| Argument address | `0x01C7FE08` | `0x01C7FE08` |
| Explicit argument prefix copy | 6 words / 24 bytes | 6 words / 24 bytes |
| Pointed flash-header address / copy | `0x01C7FE20` / 32 bytes | `0x01C7FE20` / 32 bytes |
| Indirect application call | `0x01C0528C` | `0x01C052F2` |
| Argument +80 initialization | Conditional on metadata pointer | Conditional on metadata pointer |
| Argument +88 initialization | Not established | Not established |
| `usb_update_mode` marker | Present | Present |

Twelve reviewed instruction sites in each code image are checked, including
the startup RAM/stack operands and handoff copy/call. The code images are not
byte-identical. Only 2,540 bytes match at the same offsets, but relocated code
and changed layout make that statistic unsuitable as a semantic similarity
score. No whole-loader equivalence is claimed.

The SDK bank is 14,488 bytes including its 16-byte header. Its SHA256 is
`b88747cb72afc554d6ca078594a89f4fdcea5a74d26345b633f8e2524e7e3415`;
decoded code SHA256 is
`0981b66bbf32a6be9b0e371a9a7607efb6c9260c604271e5c86046f376403750`.
FM1 decoded code SHA256 is
`fcaf033c5e10526353ecb11279a7556ac8645026d370c0267912cb4bfa7c7e4d`.

**The SDK prebuilt retains the same limited explicit argument handoff.** Merely
swapping loaders does not establish initialized SDRAM/external-app extensions.
Our no-SDRAM/no-external-app application compatibility wrapper remains justified.
“Not established” does not prove a field is never initialized on another path.
Different BSS/stack placements are normal layout differences and do not diagnose
the runtime USB stalls.

## Published user-boot source behavior

The independently compiled user-boot example does the following in `main.c`:
read `PLL_SRC`, select 48 MHz using LRC or a 24 MHz oscillator, initialize IRQs
and architecture support, mount JLFS, check upgrade paths, then call
`sfc_mode_boot()`. The bodies of key functions reside in the missing library.
Their FM1 compatibility and behavior are therefore not established by compiling
the call sites.

The project selects `USB_MODE=1`, with a custom HID updater using 64-byte
transfers. `USE_UPGRADE_MAGIC` is 0, so its explicit `user_check_upgrade()`
branch is disabled in `main.c`. If enabled, that optional branch uses an
eight-byte `uboot` start/success marker, unlike the `usb_update_mode` marker
used by our current application recovery path. `jl_check_upgrade()` remains a
separate library call; marker handling inside it is unavailable here.

The example also enables dual-bank logic and contains erase/write/copy routines.
Their flash-layout assumptions have not been qualified against FM1's preserved
single application layout. The custom HID path is distinct from FM1's observed
USB mass-storage updater. We cannot attribute that HID behavior to the SDK
prebuilt reference without further binary evidence or a bench test.

Nothing here establishes different steady-state USB audio behavior: once
application initialization has completed, its USB tasks, interrupts, FIFOs and
DMA own playback. A loader change is not a demonstrated repair for the stall.

## Reproduction and evidence

Use an external clean checkout of each pinned SDK and the already validated,
private FM1 normal boot-code file:

```powershell
C:\Python311\python.exe scripts/study_bootloader.py `
  --boot-sdk F:\dev\fm1\references\source\fw-Bootloader `
  --app-sdk F:\dev\fm1\references\source\fw-AC79_AIoT_SDK `
  --stock-code F:\dev\fm1\private-backups\readback-20260926-024637\analysis\bootloader\stock-boot-code.bin `
  --out F:\dev\fm1\private-backups\bootloader-study-20261001
C:\Python311\python.exe scripts/test_study_bootloader.py -v
C:\Python311\python.exe -O scripts/test_study_bootloader.py -v
```

Validation: eight parser/CRC/rejection tests pass in both modes. Two build runs
in separate output directories produce identical hashes for all 11 compiled
objects and identical comparison results. Compiler working-directory metadata
is held constant for that comparison. Both links fail on the same missing
archive. Source SDK checkouts remain unmodified.

Private output contains `study.json`, `build.log`, object files, and explicitly
named `*-analysis-only.elf` and linear-sweep disassembly files. Those synthetic
ELFs are views of existing code, not linked candidates. Linear sweeps include
data and undecoded instructions; they are not execution traces. Hardware boot,
updater compatibility, electrical initialization and recovery are unverified.
Vendor code/binaries and FM1 dump-derived artifacts are not committed.
