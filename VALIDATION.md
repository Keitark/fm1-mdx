# Validation record

Date:2026-09-30 (Asia/Tokyo).

## Host checks

`python scripts/build.py host` passed in the standalone repository:

- Two native CTest groups: player/karaoke/USB contracts and FM1 task lifecycle.
- Six Python checks including integration with the real C CDC line parser, with
  fragmented writes from the PC upload client.
- Original demo MDX/PDX renders to44.1kHz stereo PCM on the host.
- Muted playback cannot key-off a manually held FM note; parameters continue.
- Switching tracks, releasing an older note, unmuting, stopping and changing
  ownership release the correct voice.
- All-track mute reaches silence after envelope/sample tails drain.
- Truncated/mutated file headers, transfer offsets, oversized input, malformed
  lines, missing PDX and timeouts remain bounded or are rejected.
- The modeled board task checks PCM DMA, playback controls, STOP/upload
  exclusion, continued playback across USB disconnect and complete teardown
  before serial UBOOT.

The parent FM1 workspace also passed27 USB/NES CTest groups and11 Python
USB/static corruption checks after the shared display/parser audit changes.
Those results are regression evidence, not MDX hardware acceptance.

## Firmware candidate

The external pinned SDK and installed pi32v2 compiler are used by
`python scripts/build.py firmware`. The resulting manifest and static audit
record exact application/source/library hashes, section sizes and heap space.
No stock dump is required by this standalone build.

The pi32v2 candidate passed the full static audit:179,568 application bytes,
198,796 bytes of SDK heap before runtime allocations. Read-only demo placement,
startup ABI, delayed SDK integrity scheduling, USB/IRQ closure and board-power
instructions passed. These sizes and hashes are candidate-specific.

The application payload is not an installer, raw-flash image or firmware update
container. Build output is intentionally ignored by Git. Stock-compatible
packaging and physical installation belong to the separate FM1 bench workflow.

## Remaining bench gates

- Confirm physical button/encoder assignments and keyboard pitch.
- Observe DAC format/clocks and audition low-gain stereo output.
- Run a sustained demo and record underruns; the host does not measure the
  target's synthesis throughput or display/USB scheduling margins.
- Hold a live note through playback rests and patch changes while muted.
- Load representative external MDX/PDX songs and verify disconnect behavior.
- Verify STOP and serial UBOOT entry on hardware with the known rollback ready.

## First hardware installation

The user authorized flashing on2026-09-30. The separate existing FM1 bench
writer installed the application built from commit
`92a23169cea0f9cf0b5fe5bb80c697885e6b4d94`, with a verified NES rollback saved
privately. Application SHA256:
`8fbd8dbf270944a5bdc53555e65f435952b84c0cca6f0d5709f9810acda4858a`.

- All53 changed sectors verified, with the application directory written last.
- One full1MiB readback exactly matched the packaged candidate. Boot,
  configuration and reserved bytes were preserved.
- A single reset was issued. The existing reset CLI then raised its known text
  decoding error. Successful serial boot observation resolved the latch;
  no second reset or flash retry was issued.
- COM10 reported `MDX-KARAOKE/1`, with audio frames698048 to1140160,
  zero underruns and zero player/LCD/keys/audio error fields.
- The user confirmed both the MDX screen and demo audio work.
- The original demo MDX/PDX bundle was uploaded into RAM over CDC, then played.
  Selection, track0 mute, manual C4 on/off, unmute and return to the flash demo
  were acknowledged. While muted, frames advanced18894400 to19026816,
  with zero underruns and errors. This confirms control/transport operation;
  the held-note/patch-change sound still needs a focused listening test.
- The player was left running the built-in flash demo with no tracks muted.

Reopening CDC exposed occasional lost first commands and partial diagnostic
lines. The PC client now allows DTR to settle, accepts a complete firmware
identity line, and retries only read-only HELLO up to three times. Song/control
commands are never automatically retried. Host checks pass after this fix.

The private deployment receipts, serial log and rollback remain in the parent
FM1 bench workspace; no unit-specific images are committed here.

## Panel correction candidate

The user found FX toggles karaoke, SEL toggles playback and OCT−/OCT+ do not
shift pitch in the first image. The correction moves playback to panel slot12,
following the user's stated button order; OCT−/OCT+ on slots0/1 now shift new
keyboard notes by12 semitones per press, bounded to−3..+2 octaves. FX remains
karaoke and encoder0 is assigned to the SELECT knob for track selection.
`MDX INPUT` reports panel slots and all encoder counters for physical checks.

Host task tests cover PLAY/STOP, unused SEL, octave bounds, held-note release
after shifting, older-key releases, and encoder0 track selection. Both CTest
groups and six Python checks passed. The linked180,272-byte application passed
the static audit with SHA256
`386b9a1fee54d7c9e5e3953cdf6e11bd04cae38ee6b9e50856f6ac028d5696dc`.
The unchanged startup wrapper writes its trace at `ota_status+336`; that exact
store was reviewed before adding it to the auditor's encoding allowlist.
The correction from commit `2f71048` was installed with45 verified sectors and
one exact full1MiB readback. The previous MDX image and original NES rollback
remain saved privately. A single reset and successful MDX serial observation
confirmed frames521600 to962816, zero underruns and zero reported errors.
MDX STOP followed by the existing guarded UBOOT arm/confirm handshake also
verified complete peripheral teardown on the first MDX image before this write.
Physical panel/knob confirmation of the correction is pending.

Persistent uploaded songs, drag-and-drop mass storage and MIDI input remain
unimplemented. PCM playback uses bounded48kHz-to44.1kHz sample resampling;
high-quality PDX interpolation and broad MDX compatibility need further tests.

## Optional composite USB audio candidate

The UAC1 + CDC profile is source-built and unflashed. Four native CTest groups,
six Python client checks and the actual descriptor-tree check pass. Audio tests
simulate60 seconds at0 and±500ppm DAC clock drift with no FIFO underrun/overrun
after priming. Callback tests cover DMA alignment/bounds, persistent EP0 replies,
interface alternate settings, reset and complete audio disarm before UBOOT.

Both profiles link and pass the static audit. The CDC-only application remains
byte-identical to the currently installed control candidate:180,272 bytes,
SHA256 `386b9a1fee54d7c9e5e3953cdf6e11bd04cae38ee6b9e50856f6ac028d5696dc`.
The composite application is183,088 bytes with189,932 bytes of SDK heap before
runtime allocations, SHA256
`f8be2cc67c29ba1b2126e0a858d3bcc7009d8cb9d5d68a22c42e4890b2fa0dda`.

The composite auditor checks the173-byte UAC descriptor,512-byte aligned
internal-RAM DMA pool and required audio hooks. LTO moved the boot trace to
`ota_status+344`, the power gateway state to+28, and power state operands to
+224/+236/+260. These exact destinations and instruction encodings were checked
against symbols and disassembly. The normalized1,082-byte SDK power initializer
still matches its reviewed hash. Corruption tests reject changed descriptors,
power destinations and gateway instructions.

Hardware enumeration, duplex sound, conversion quality, latency, scheduling
headroom, concurrent CDC operation and recovery are pending. No USB audio
performance claim follows from the offline checks. See[USB_AUDIO.md](USB_AUDIO.md).

## Screenshot-enabled candidates

The CDC screenshot feature freezes the last completed240x240 UI frame and
regenerates packed pixels with the same renderer that writes the LCD. It adds
about512 bytes of state, with no full framebuffer. CRC preparation yields every
eight rows and does not hold the control lock while rendering. The client
validates transfer tokens, offsets, lengths and CRC before saving the PNG.

Five native CTest groups, six client tests, three screenshot Python tests and
the UAC descriptor test pass. Tests compare every captured pixel with LCD row
output, keep captures immutable across UI changes, reject stale/malformed
requests and corruption, validate PNG chunk CRCs and check scheduling yields.
A host fixture PNG was decoded and visually inspected; it is not device evidence.

Both screenshot-enabled profiles pass their static audits:

- CDC:182,160 application bytes,198,188 bytes of pre-allocation heap; SHA256
  `5ca92c98a62bc544921233eab27c2f3025dbbd64fec6301785543eea7e463d52`.
- Composite audio + CDC:184,784 application bytes,189,420 bytes of pre-allocation
  heap; SHA256 `046be484aacdc008974abfd65eeefd762eb541e2560048506e3eb85689fc0b90`.

Reviewed trace offsets are+340 for CDC and+348 for composite. Reviewed LRC/LRC
callback/low-power operand tuples are+220/+232/+256 and+228/+240/+264 respectively.
The normalized SDK power hash remains unchanged; linked-ELF descriptor and power
corruption tests still reject changes. These candidates have not been flashed.
Live screen matching and audio continuity during capture remain bench gates.
See[SCREENSHOT.md](SCREENSHOT.md) for commands and capture semantics.
