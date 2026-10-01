# FM1 clean-lab boot project

This is an offline source lab for application validation and startup inspection. It uses original
portable C, arbitrary synthetic fixtures and ordinary build tools. It does not
query FM1, interrupt playback, prepare an installation image or replace its
bootloader. A host pass or WL82 object is **not a bootable firmware release**.

## Step 1: isolate and verify the portable core

The lab builds only `../bootloader/boot_validation.c` and its public types.
The earlier `boot_policy.c` handoff proposal, 92-byte compatibility profile,
stock comparison tooling and all board code are excluded. There is one shared
validation implementation; the earlier handoff tests continue using it.

The descriptor is an in-memory lab contract, not a vendor file format. Caller
supplied values define flash bounds, allocation, XIP mapping, entry and CRC.
The lab accepts only version 1, zero features, at least two payload bytes,
an even in-bounds entry and a matching CRC16/XMODEM. CRC detects corruption;
it does not authenticate firmware. Accepting a descriptor means eligibility
for a future boot adapter, not permission to jump or write flash.

```powershell
python scripts/build_bootlab.py host
```

Three host tests run: exhaustive offset/boundary validation on a 4096-byte
synthetic flash, three visible scenarios (valid, corrupt, outside its
allocation), and guarded BSS-span clearing plus the startup fixture. Eleven Python
audit-rule tests also run. Host tests do not execute target instructions or use
a song, board identity or backup.
The generated `build/bootlab/host/manifest.json` lists source hashes, the
successful/failed status and excluded inputs. It is reset before every build
so a failed attempt cannot leave an earlier successful manifest as current.

Optional target object compilation, with an externally installed compiler:

```powershell
python scripts/build_bootlab.py wl82 --toolchain C:\JL\pi32\bin
```

This compiles the validation module as freestanding pi32v2/r3 C and rejects any
undefined symbols. It records compiler/tool and object hashes. It does not link
an entry point, use a ROM export table, make a boot bank or call a downloader.
Host tests require Python, CMake and a C11 compiler; the optional WL82 object
requires Jieli's compiler and symbol tool. No SDK checkout or archive is needed.

## Input and provenance boundary

| Input | Lab use |
|---|---|
| Original validation code and synthetic tests | Compiled and tested |
| External host compiler/CMake/Python | Build tools |
| External Jieli compiler/nm/linker/objdump | Optional offline target tools |
| Public WL82 RAM-window declaration | Step 2 linker origin/length reference |
| SDK boot interface declarations | Reserved for later ABI/hardware steps |
| Vendor `uboot.a` / prebuilt loader | Excluded |
| FM1 dump, configuration, identity or calibration | Excluded |
| Stock-derived addresses/disassembly and comparison scripts | Excluded |
| Hardware, flash writer or updater | Excluded |

"Clean-lab" here means a controlled-input, reproducible source lab. The author
has already examined SDK and stock-loader material earlier in this project;
this is not a claim of independent clean-room reverse engineering. Synthetic
addresses in the fixtures do not describe FM1's actual layout. Original project
source retains the repository's GPL-3.0-or-later notice. Future dependencies and
their provenance must be recorded before they enter the lab.

## Step 2: entry and linker inspection

```powershell
python scripts/build_bootlab.py wl82-entry --toolchain C:\JL\pi32\bin
```

This compiles original `entry.S`, `entry.c`, `startup.c` and the shared validation
core, then links with `ram.ld`. No SDK checkout, vendor archive, ROM exports,
standard library or downloader is used. The linker rejects unresolved symbols
and orphan sections. Outputs are `build/bootlab/wl82-entry/bootlab.elf`, a map,
disassembly and `manifest.json`; they are inspection artifacts, not a boot bank
or installable image. The builder has no packaging/device commands.

The RAM-window reference is Jieli's public
[WL82 ram_ld.c](https://github.com/Jieli-Tech/fw-Bootloader/blob/ff3b9299d1b94d834b995fd858b3f75e2faa9840/user_boot/cpu/wl82/output/ram_ld.c):
origin `0x01c02000`, length 128 KiB. Our section organization, two 2048-byte
stacks, 32-byte bound alignment, RAM breadcrumbs and park behavior are original
lab design choices. Their sizes and addresses are **not confirmed FM1 ROM
reservations or a measured stack high-water mark**. Nothing writes the SDK's
separately declared interrupt-vector area, and no vector table is installed.

Entry establishes SP and SSP, calls C to clear only the linked BSS interval,
runs valid/corrupt/out-of-allocation synthetic validation, writes `lab_result`
(expected 7) and `lab_finished` (expected `0x424c4142`) in RAM, then returns to
a `nop`/branch park loop. These RAM values have not been observed on hardware.
Code, read-only constants and any initialized data share the loadable RAM
segment, so there is no flash-to-RAM data-copy adapter in this milestone.

The entry requires code/data already loaded into RAM, a quiescent caller and
suitable CPU/cache state. It does not establish those prerequisites, disable
interrupts, reset peripherals, configure clock/cache/SFC, read flash metadata
or jump to an application. Separate stack reservations and the compiler's C
calling convention do not prove the ROM's entry ABI. No CPU emulation or target
execution is claimed.

`scripts/audit_bootlab.py` independently reads ELF sections, symbols and load
segments. It checks the RAM window/entry, coverage, permissions, non-overlap,
BSS/stack NOBITS intervals and alignment, breadcrumbs, resolved symbols and
entry stack immediates. Disassembly checks SP/SSP before C, BSS clearing before
the fixture and return to the park loop. Its byte-shape checks intentionally
depend on this pi32v2 toolchain; a new compiler requires review. The audit is
a layout/order check, not a proof of every instruction's runtime behavior.
The target build runs twelve Python tests, including nine corruptions of the
actual ELF (header, entry, load extent, BSS type, stack immediate, park bytes,
unresolved symbol, section-table offset and truncation). Checks use explicit
exceptions and remain active with Python optimization.

Host tests poison 129 different BSS lengths and verify every adjacent byte,
including simulated stack space, survives. They exercise the exact portable
clear/fixture functions, not the WL82 assembly or ROM. Manifests are invalidated
before tool execution and record FAILED on any compiler/link/audit failure;
older ELF files may remain in the output directory and must not be used when
the current manifest is failed.

## Following milestones, one at a time

1. **Portable validation lab:** build/test scenarios and target core object.
2. **Target entry and linker scaffold:** derive memory/stack reservations from
   public WL82 interfaces and explicit lab assumptions; link and inspect an offline executable. Record the
   unresolved ROM-entry ABI explicitly; a linked scaffold alone cannot boot.
3. **Hardware initialization:** establish exact clock, interrupt, cache and
   flash-access contracts; implement original adapters for documented behavior.
4. **Application handoff:** acquire/validate local board metadata, map decoded
   application bytes and verify the public SDK ABI. Do not invent undocumented
   extension fields or reuse unknown ROM addresses.
5. **Packaging and recovery:** define a reproducible image path without private
   backups, an independent recovery entry, bounded update writes and interrupted
   update behavior. Preserve each unit's local identity/calibration.
6. **Bench qualification:** inspect one complete candidate and recovery plan,
   obtain authorization for that exact image, then test cold boot, recovery,
   playback and USB on hardware. The existing application-only protected writer
   cannot install a replacement bootloader.

Each milestone needs its own evidence before it is called complete. SDK
declarations alone do not establish missing initialization implementations.
The player still separately depends on its pinned SDK libraries/toolchain.

## Step 1 results (2026-10-01)

Both standalone host tests pass, including all 4096 fixture offsets and the
three visible scenarios. The WL82 core compiles with warnings treated as
errors, produces a 732-byte object and has no undefined symbols. The existing
validation/handoff host test also passes after the module split. No target
startup/hardware code has been added and no device was operated in this step.

## Step 2 results (2026-10-01)

Three standalone C tests and eleven Python audit-contract tests pass. The
freestanding pi32v2/r3 scaffold links without undefined symbols or vendor
archives. Its layout uses 4672 bytes of the provisional RAM window: 544 bytes
of loaded code/constants including alignment, 32 bytes of BSS, and 4096 bytes
of stack reservations. Entry/disassembly inspection and nine deliberate
linked-ELF corruptions pass. This is offline link/inspection evidence; target
execution, ROM boot, recovery, clock/cache/interrupt state and stack capacity
remain unqualified. No FM1 operation or installed-firmware change occurred.
Two output directories produce identical ELF SHA256
`5f9bea0883ecbd24f1801b19e9b0fd46193995bdf79f6ec131b9fe3097aad3c1`.
The target audit tests also pass with `python -O`. A missing-toolchain attempt
after a successful build produces a nonzero exit and a FAILED/nonflashable
manifest, even though the old ELF remains on disk.
