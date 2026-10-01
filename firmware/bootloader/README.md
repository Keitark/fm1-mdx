# Source bootloader work

This directory begins our own bootloader implementation for a public FM1
firmware project. **It cannot boot or update a device yet.** The implemented
piece is application validation and preparation of the application handoff.
It uses no vendor archive, stock image, unit backup, device I/O, or SDK header.
It is not linked into the player and does not change the installed firmware.
The [clean-lab project](../bootlab/README.md) separately builds portable
validation and an offline linked entry/stack/BSS scaffold, excluding this
directory's provisional handoff implementation. That scaffold does not
establish the ROM-entry ABI or initialize hardware.

The current working assumption is that the public player may retain its pinned
Jieli SDK and compiler dependencies. Replacing the bootloader alone does not
replace the player's closed SDK libraries or provide a complete installation
package. A completely vendor-independent firmware/toolchain is a larger task.

## Implemented contract

`boot_validation.c` and `boot_policy.c` implement the following, with synthetic tests:

- Check caller-supplied physical flash bounds, application allocation, maximum
  application size, and XIP mapping arithmetic without integer wraparound.
- Accept only descriptor version 1 and zero feature flags. This initial profile
  has no SDRAM or external application.
- Check that an even application entry leaves at least two bytes inside the
  mapped application. Compute CRC16/XMODEM over the decoded payload and reject
  a mismatch. CRC detects corruption; it is not signature authentication.
- Construct the 24-byte `BOOT_DEVICE_INFO` prefix from local board metadata,
  zero all remaining bytes of a 92-byte argument area, and append a separate
  32-byte flash header. The prefix points to the appended header, leaving
  extension offsets +80 and +88 zero.
- Reject an unaligned or out-of-range target handoff address without changing
  the caller's output buffer.

The common prefix is based on the pinned AC79 SDK's
`include_lib/system/boot.h`; the extension-zeroing profile follows our existing
no-SDRAM/no-external-app compatibility wrapper. The layout is a **candidate
contract**, not proof that a new loader will satisfy every ROM/SDK requirement.
The stock/reference comparison remains in [BOOTLOADER_STUDY.md](../../BOOTLOADER_STUDY.md).

The application descriptor and policy are in-memory inputs, **not a specified
on-flash format**. A future adapter must validate flash metadata, obtain real
chip/trim/MAC information, supply decoded bytes and establish the XIP mapping.
Never replace calibration or identity with arbitrary defaults. The caller must
validate immediately before handoff and prevent changes until control passes.
For handoff placement, supply a usable RAM interval excluding ROM state,
interrupt tables, stacks and loader allocations. The core prepares bytes; it
does not set stacks, disable interrupts, flush caches, or jump to the app.

## Build without SDK or hardware

With CMake and a C11 compiler:

```powershell
cmake -S firmware/bootloader -B build/boot-policy
cmake --build build/boot-policy --config Release
ctest --test-dir build/boot-policy -C Release --output-on-failure
```

The normal `python scripts/build.py host` command also runs `source_boot_policy`
alongside the existing player and USB tests.

Optional WL82 object compilation, using an externally installed Jieli compiler:

```powershell
New-Item -ItemType Directory -Force build/boot-policy-target | Out-Null
& 'C:\JL\pi32\bin\clang.exe' -target pi32v2 -mcpu=r3 -integrated-as `
  -std=c11 -Oz -fno-common -Werror -I firmware/bootloader `
  -c firmware/bootloader/boot_validation.c -o build/boot-policy-target/boot_validation.o
& 'C:\JL\pi32\bin\llvm-nm.exe' --undefined-only build/boot-policy-target/boot_validation.o
```

The reviewed compiler produces an object with no undefined symbols, including
no hidden `memcpy`/`memset` dependency. This is **object compilation only**;
this directory does not supply a target reset entry or flashable bank. The
separate lab can link an inspection scaffold, not a complete bootloader.

## Remaining work for a complete public firmware release

| Piece | Current status | Required evidence |
|---|---|---|
| Validation and handoff policy | Source implemented; host tested; WL82 object compiles | Integrate with actual target adapter |
| ROM-to-loader entry and RAM layout | Offline lab scaffold linked; ROM compatibility unqualified | Reset calling convention, actual reserved RAM, stack capacity and inherited state |
| Clock, IRQ and cache startup | Unimplemented | Exact WL82 initialization and inherited ROM-state requirements |
| Flash reads and application XIP mapping | Unimplemented | SFC setup, decoded-byte access and verified mapping/cache behavior |
| Local board metadata | Unimplemented | Validated flash header, chip identity and calibration acquisition |
| Public image format and packager | Unimplemented | Source-built boot bank/header CRC and application format/layout; no private dump dependency |
| Independent recovery/updater | Unimplemented | Recovery entry independent of a stalled app, bounded writes, interrupted-update behavior |
| Public installer and hardware qualification | Unimplemented | Exact image/layout checks, preserved local identity/configuration, recovery and cold-boot tests |
| Player SDK dependencies | Existing external pinned dependency | Explicit source/build provenance and redistribution review for a release package |

The SDK documents, declarations and binary comparison do not establish all
target initialization and ROM-entry contracts above. No undocumented ROM
address or other chip family's boot library is substituted. Document each
required contract before implementing it; then link and inspect an offline
candidate before proposing separately authorized hardware tests.

Public source and host checks can be shared now. Do not describe this directory
as a complete bootloader or distribute a private full-device backup as a public
firmware image. The existing protected application updater cannot install a
new bootloader; its boot-region protection remains in place.

## Validation at this milestone (2026-10-01)

- Full host suite: 13 CTests, 7 client tests, 4 screenshot tests and 1 descriptor
  test pass. The boot-policy test covers CRC corruption, allocation/mapping
  overflow, entry/alignment, the full handoff layout and unchanged output on
  rejected placement.
- After the final volatile-qualifier correction: focused boot-policy host test
  passes; WL82 object compilation with `-Werror` succeeds and its undefined
symbol list is empty. The later clean-lab split keeps the validation module
self-contained; the separate handoff module now calls its shared layout check.
- No device query, reset, transfer, flash or hardware acceptance was performed
  for this milestone.
