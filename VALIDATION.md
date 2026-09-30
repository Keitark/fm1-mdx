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

## Composite installation and recording-terminal correction

The user authorized the combined USB audio/screenshot flash on2026-09-30.
The separate reviewed writer installed commit`0c00ef3`, with46 verified sectors
and an exact1MiB readback, SHA256
`5805f8f78b3d3994b5f67951072ae3f30650d49e9fca0727912bfb4f6e270788`.
The previous control firmware rollback remains saved privately. One reset and
successful serial observation confirmed MDX playback onCOM4, frames1,472,320
to1,913,536, with zero underruns or player/peripheral errors.

Windows enumerated healthy CDC, composite and USB audio devices, plus a playback
endpoint. A real240x240 screenshot transferred and decoded; subsequent MDX
status still showed zero underruns. No recording endpoint appeared in PnP or
WinMM. The capture terminal was Input Undefined(0x0200).

Issue#7 changes that source to the defined embedded Synthesizer terminal0x0713
and advances composite bcdDevice to2.02. The source still records the MDX synth;
no analog input is added. Host and linked corruption checks pass. The corrected
184,784-byte application's SHA256 is
`117dc6e2d2f62806ea57ff645d635fd4c9d844baedd62c7e4e124bc5caad80d3`.
Its hardware recording verification is pending.

## Current installed profile and OutRun glitch investigation (issue9)

The later virtual-Line capture profile at`9fb7041` was installed with exact
1MiB readback SHA256`414c83b57df63faa763a339d9f58230af2459b712c5d9ade8818a3a070480393`.
Windows already exposed a valid stereo48kHz capture pin, but its endpoint was
disabled/hidden. Enabling only the FM1 endpoint made recording work; terminal
classification alone is not established as the cause. A30-second real capture
contains1,440,000 stereo frames. USB recording precedes the analog master volume
and excludes PC return. The user confirmed the demo's muted PCM drum removes
the metronome-like click. Commercial songs and recordings remain private.

Before the issue9 correction, live counters showed MDX underruns0, capture
underruns0 and PC playback underruns2. A separate30-second input-only OutRun
capture added no underruns in any direction. These counters cannot exclude
late hardware DMA service or synthesis discontinuities.

The MDX consumer incorrectly reset priming after a successful callback that
consumed exactly the last64 samples. The next callback then waited for1470
samples, creating an uncounted gap even if the producer had already supplied
another full block. The regression now requires continuous output across that
boundary; actual missing samples and subsequent rebuffer silence are counted
separately. Callback timing and render/queue diagnostics are also available.

Issue9 also interpolates the native48kHz PCM timeline into44.1kHz, gates UI and
sleep using the post-render reserve, and ramps PC playback at stream starvation
and recovery. It uses RetroFM's measured PCM/FM ratio49700/32768 with the
existing common1/2 gain. It does not copy the separate X68Sound PCM trim into
this different decoder. A matched60-second private OutRun host comparison
measures PCM RMS+3.59dB, PCM peak3415 and mixed peak6424, with no clipped samples.
This was a starting balance derived from the earlier implementation. The native
MXDRV comparison below supersedes it; physical listening acceptance stays open.

Host regressions exercise exact-empty versus genuinely starved DMA buffers,
rebuffer accounting, delayed callbacks, signed PCM interpolation, saturation
and nonzero PC underrun/recovery/stream-stop continuity. The linked power audit
retains the normalized SDK hash and checks the reviewed diagnostic layout
trace+380 and LRC/LRC/low-power+260/+272/+296, including corruption rejection.
The user approved this candidate, and commit`9e223de` was flashed with45 verified
sectors and exact1MiB readback SHA256
`5ff7edf9b1e97f3821cbbcf79cc97e408b38a8e2274d55e29d6ca31d35d9acb4`.
One reset was sent; the reset CLI's text decoding error was resolved by
successful observation, without another reset. The pre-fix composite rollback
remains preserved separately.

OutRun was reloaded into volatile RAM with all tracks unmuted. New live counters
confirm genuine MDX buffer underruns and rebuffering, while the USB capture
underrun/overrun and packet-error counters remain zero. A bounded input-only capture produced1,440,000 stereo48kHz frames. Over the
31.7-second surrounding observation interval, MDX added6,080 missing frames,
95 rebuffer events and154,816 rebuffer-silence frames. USB capture added zero
underruns, overruns or bad packets. A fresh240x240 completed
screen shows PLAY / USB RAM. The first correction is not sufficient for audible
acceptance. The old zero counter concealed gaps rather than proving headroom.

Linked disassembly also proves the timing clock is `jiffies*10`, with10ms
resolution. The first diagnostic's4ms late threshold therefore counted normal
tick crossings. The follow-up threshold is20ms; sub-tick DMA deadlines remain
unmeasured.

The next candidate replaces per-sample software-double cubic output shaping
with the same integer expression. All131,073 tested values from-65536 to65536
match exactly, including the preserved upstream out-of-range behavior. A
matched60-second OutRun render is byte-identical to the preceding candidate:
2,646,000 frames, WAV SHA256
`f760dcb957919c0c097f439a107df3a935b06e57ec3b71aefd2813a13ae94d46`.
Six CTest groups, client/screenshot/descriptor checks and five linked corruption
checks pass. The static audit needs no new instruction/layout exceptions.
This optimization removes software floating-point work from output shaping;
its deployment and hardware results are recorded below.

## Integer output optimization installation

The user approved the exact follow-up image on2026-09-30. Code at`61eb729`
was installed with45 verified sectors and one exact1MiB readback, SHA256
`1c671a1c09840f15d0dea5411d3cdbd04febc1dee6b9551a969f2b15e85c36e6`.
Application SHA256 is
`0af9e0eadf6b2fa120553c3f30d94df5d2621ed0ba7b642570a14cfe9d479f31`.
Both earlier composite images remain saved privately. One reset was sent;
successful serial observation resolved the known reset-log decoding error.
Demo frames advanced543,104 to984,320 with zero underruns or peripheral errors.

OutRun was reloaded into volatile RAM with all tracks unmuted. A30-second
input-only USB capture produced1,440,000 stereo48kHz frames with peak6111 and
no synth underruns, rebuffer events, rebuffer-silence frames, USB underruns,
overruns or bad packets. During the same capture, the CRC-checked240x240 screen
transfer completed and showed PLAY / USB RAM. Minimum primed fill stayed960
frames; coarse callback maximum10ms and late count0 reflect the SDK's10ms clock,
not a sub-tick hardware deadline measurement. Physical listening acceptance
remains separate from these counters and the USB recording path.

A subsequent120-second input-only recording produced5,760,000 stereo frames,
peak7376 and no clipped samples. Five streaming snapshots and the final status
show zero MDX underruns, rebuffer events or rebuffer-silence frames. USB capture
added120,011 packets with zero underruns, overruns or malformed packets; primed
minimum fill remained960 frames. No capture script opened PC playback. Between
the two recordings, a separate brief PC return supplied3,700 packets and
registered two PC playback underruns; both values stayed unchanged throughout
the120-second test. They do not describe the MDX or USB capture buffers.

## Native MXDRV balance candidate (2026-09-30)

The user's listening comparison found PCM substantially louder in native
MXDRV/X68Sound. Isolated120-second OutRun stems at44.1kHz stereo16-bit confirm
it: installed FM1 PCM RMS325.55 versus native MXDRV1304.44 (+12.056dB), while
FM RMS1317.53 versus2506.69 differs by5.587dB. PCM is therefore6.469dB lower
relative to FM. Isolation is verified by recombining stems: FM1 is exact and
MXDRV differs by at most1LSB. MXDRV resets its channel mask at Play, so reference
isolation is applied and read back after Play.

The new candidate uses PCM output gain8/5 instead of49700/65536 and keeps FM
at1/2. Its integer product is bounded within32 bits and requires no floating
point or64-bit division. A new120-second host render measures PCM+6.489dB,
within0.020dB of the native MXDRV PCM/FM RMS ratio. FM-only output is byte-identical
to the installed source; mixed peak9707 and zero clipped samples leave headroom
for the tested song. This is OutRun balance calibration, not equivalence between
the different emulators, filter responses or volume curves.

Host6 CTests,7 client checks,3 screenshot checks and the descriptor check pass.
The composite board link, static audit and all5 linked corruption tests pass.
Application payload185488 bytes, SHA256
`ae04821634c56876fd308e99a30b3d8c1c631c4f0c73e07ea85468e32d57066a`.
Offline unit-image packaging against the observed installed baseline succeeds:
candidate1MiB SHA256
`06c55a3ea1310b91fff144949e6906823e697c305683d89d3979292794c51d11`.
The installed image remains
`1c671a1c09840f15d0dea5411d3cdbd04febc1dee6b9551a969f2b15e85c36e6`,
and its exact rollback is retained privately. The new candidate has not been
flashed; exact-candidate authorization and bench playback/counters remain pending.
All recordings, songs, stems and unit images remain outside Git.