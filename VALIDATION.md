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
The user approved the exact candidate and rollback plan. The candidate was
flashed with44 verified sectors and one matching full-image readback, reset
once, and observed booting. The preceding image
`1c671a1c09840f15d0dea5411d3cdbd04febc1dee6b9551a969f2b15e85c36e6`
is retained privately as rollback. Listening acceptance FAILED: the user heard
initially clean playback become noisy through USB audio on the PC. Playback
was stopped. The first120-second capture was aborted before its WAV was saved;
it is not a completed bench pass. A subsequent149.98-second incremental capture
has peak32768 and RMS18612. A one-byte alignment change reduces these to
peak10323 and RMS1547. Sampled buffer-error counters remained zero. This is
strong evidence of sample alignment/order corruption in the capture chain,
not proof of its exact origin; analog output was not evaluated for this failure.
All recordings, songs, stems and unit images remain outside Git.

## Integrated host replay of the noisy candidate (2026-09-30)

`mdx_audio_replay` executes the current owner task, sequencer/FM/PCM renderer,
2048-frame ring, actual64-frame `audio_output`, UAC target/bridge and little-endian
packet encoder. The SDK boundaries use mocks with a10ms OS tick,64-frame DAC
callbacks and1ms USB callbacks. Each encoded sample is independently decoded
and checked against the maximum source magnitude, so passing buffer counters
alone cannot conceal byte-swapped full-scale noise.

Build with `python scripts/build.py host`. For a private external song:

```powershell
& firmware/mdx/build/host/Release/mdx_audio_replay.exe song.mdx song.pdx replay.wav 600 0 normal
& firmware/mdx/build/host/Release/mdx_audio_replay.exe song.mdx song.pdx stress.wav 180 500 stress
```

Duration is1..3600 seconds; drift is-1000..1000ppm. Runs of600 seconds or longer
queue PLAY again every360 seconds to cover song restart after natural completion.
Stress mode seeds ring/FIFO counters near32-bit rollover, closes/reopens capture
every15 seconds, resets the USB interfaces every61 seconds, and stalls the
producer60ms every47 seconds. These are explicitly injected conditions.

Private OutRun results against the current playback sources:

| Run | Duration | Peak | Clipped samples | Synth missing / rebuffer events | USB FIFO errors |
| --- | --- | --- | --- | --- | --- |
| Normal,0ppm; one song restart | 600s | 10785 | 0 | 0 / 0 | 0 |
| Normal,+500ppm | 180s | 10410 | 0 | 0 / 0 | 0 |
| Normal,-500ppm | 180s | 10555 | 0 | 0 / 0 | 0 |
| Stress,+500ppm | 180s | 10303 | 0 | 192 / 3 | 0 |
| Stress,-500ppm | 180s | 10396 | 0 | 192 / 3 | 0 |

Both180-second stress runs include14 stream restarts, three producer stalls
and ring/FIFO rollover. Deliberate starvation produces counted gaps, without
the persistent full-scale collapse. A separate600-second MSVC AddressSanitizer
stress run reports no memory errors,49 stream restarts,12 producer stalls,
768 missing samples/12 rebuffer events, and zero USB FIFO errors.

A private positive control removes exactly one byte downstream of the modeled
encoder at30 seconds. Its checked5-second interval changes from peak8589,
RMS1501 and zero clipping to peak32768, RMS18210 and426 full-scale samples.
Realigning the bytes restores peak8589/RMS1501 with zero clipping. This matches
the observed failure pattern but is an injected transport fault, not a
spontaneous reproduction by the firmware code.

All7 CTests,7 client checks,3 screenshot checks and the descriptor check pass.
The new CI test uses the redistributable demo for65 seconds of stress. Only host
test sources and documentation changed; playback/USB firmware sources remain
unchanged from935e9b7. No additional firmware was flashed for this experiment.

The model does not emulate WL82 instructions, DMA, actual USB transfers, Windows
audio drivers, or interrupt/CPU timing. Those remain possible failure locations.
The current UAC target ignores `usb_g_iso_write` results; `tx_packets` counts
encoder attempts before submission, and `bad_packets` concerns USB OUT input.
Consequently, the existing status does not certify successful or correctly
aligned USB IN transfers. Device listening acceptance remains open.


## 2026-09-30 Pocket-style UI candidate (issue12)

The UI adds real256-point stereo FFT analysis, FM8/PCM8 sample-peak meters,
20Hz requested dirty-row refresh, and completed-frame FPS telemetry. All8 CTests,
7 client checks,4 screenshot checks and the descriptor test pass. The integrated
replay checks each committed screen against its modeled LCD pixel RAM.

Private Super Laydock at+500ppm over180 simulated seconds completes3598 views
(19.99FPS),83094 row writes and780426 skips, with no missing synth samples,
rebuffers or modeled USB FIFO errors. The wire-cost model assumes400us per row;
PI32 CPU/render costs and real USB/DMA timing remain outside the model. The
10-second demo WAV is byte-identical before/after the sample meter taps.

The composite PI32 link and5 linked corruption regressions pass with the
reviewed startup/power relocations. Source-only display work remains separate
from the installed image; no UI firmware flash or hardware FPS acceptance has
occurred. See[DISPLAY.md](DISPLAY.md) for definitions and bench requirements.


### UI flash bench failure and correction

User-approved UI image00032c4e9fc259e400808023161ebb9b8f7e0eeb8304ec7864ee128b47ce649f
was flashed in47 directory-last sectors. Its complete1MiB readback matches.
The reset was sent once; the CLI logger hit its known UTF-8 decoding error.
CDC boot/audio progress is observed, but the protected observation fails on
`keys=-3`; the original session failure latch is retained. The user performed
a cold power cycle and the same scanner error/internal-audio mute persists.
Bench display telemetry is6.7-6.8FPS, maximum frame270ms; audio reports zero
underruns, no rebuffers and a10ms maximum coarse-clock callback gap.

The follow-up separates ADC volume sampling from scanner failure, adjusts the
watchdog for the actual10ms clock quantum and reduces per-row formatting/work.
All9 CTests,7 client checks,4 screenshot checks and the descriptor test pass,
including the real scanner driver at the quantized-clock boundary and an injected
scanner fault that must leave ADC volume sampling operational. The optimized
Super Laydock preview is byte-identical in indexed pixels. The correction links
with the pinned SDK and passes5 linked corruption regressions. It is not flashed
or hardware accepted yet; its exact candidate and rollback need approval.


### 2026-10-01 correction bench and zero-based LCD rows

Image0284ee8 (2b38d55bebba6a8ab8076d214d5d26da1fc9f9e125b791ec10a01255c6f53832)
was fully readback-verified and boot-observed. Live demo diagnostics show20FPS,
keys=0, valid/progressing volume ADC, zero reported synth underruns/rebuffering,
and no peripheral errors. The user confirms internal speaker output. USB
transport diagnostics now count actual SDK submissions, short returns and busy
endpoints; these counters still cannot certify received Windows PCM alignment.
A Laydock transfer timed out, then a paced retry succeeded while capture was
inactive. Concurrent capture/CDC failures remain unresolved under issue9.

The MDX row writer had reintroduced y+40 despite the already qualified NES
zero-based path. Commit4cc306d corrects it to y. Host regressions assert all240
physical RASET addresses and compare each committed LCD image without offset
compensation in the mock. All9 CTests,7 client checks,4 screenshot checks, the
descriptor check and5 linked corruption checks pass; the board link passes.

Imageca04b4e9bef7caecc60d15e182fc997994f8f7fc794fd54f83ffe43ecc4e1cc2
was installed in44 directory-last sectors and its full1MiB readback matches.
Reset was sent once; successful CDC boot observation resolved the logger error.
Frames advanced708672 to1149760 with zero reported underruns/peripheral errors.
The user explicitly confirms correct screen positioning at the top and bottom.
The50ms requested view interval is unchanged. Both prior images are preserved
privately for rollback. Physical screen offset is accepted; meter movement,
Laydock reload and USB audio reliability are separate acceptance items. Reload
at120-byte/20ms and48-byte/50ms pacing timed out with capture active.

### 2026-10-01 USB packet and lifecycle candidate

Installed c9810a47 firmware received Ray Force successfully with capture closed.
The subsequent input-only48kHz stereo16-bit recording lost both audio and CDC
heartbeats after75.62 captured seconds. COM4 then failed configuration with
Windows error31 even after capture closed. The partial WAV has peak10395 and
zero clipped samples. This establishes a shared USB failure, not its exact cause.

The new candidate replaces normal audio-IN/CDC-IN SDK polling writes with an
audited single-packet commit, preserves pending capture packets on rejection,
and invalidates them at stream transitions. The composite USB and peripheral
tasks are pinned to CPU0 with the SDK's task-name prefix, matching USB/ALINK
interrupt affinity. No synth, mix, sample-rate conversion or LPF changes are
included. The prior meter-colour correction is inherited from the parent branch.

All10 CTests,7 client checks,4 screenshot checks and the descriptor test pass.
The packet helper receives100000 mixed submissions under the mocked registers.
The pinned SDK composite link and10 linked corruption checks pass, including
packet count/commit/IRQ restoration and both task affinity registrations.
Reviewed SDK register accesses retain finite hardware-acknowledgement waits;
this does not certify a hard real-time controller deadline.

The private Ray Force host replay runs140 seconds with zero synth missing
samples, rebuffers or modeled USB FIFO errors. A360-second stress replay covers
29 stream restart/reset operations and7 deliberately injected producer stalls;
those stalls cause7 rebuffers/448 missing frames, with zero USB FIFO errors or
clipping. Host mocks do not reproduce the WL82 controller, DMA, dual-core
scheduler or Windows USB driver.

This candidate is unflashed. Hardware acceptance requires a complete Ray Force
capture, repeated OBS source activation/deactivation and mute/unmute, concurrent
CDC diagnostics/upload, USB reconnect and protected UBOOT recovery. CPU0 task
consolidation also requires checking synth reserve, internal audio and screen
FPS. Preserve the full readback-verified c9810a47 image for rollback and obtain
authorization for the exact newly packaged image before physical flashing.

### Host stall reproduction attempt

The shared endpoint harness executes actual `target.c` and `packet.c`, with
mocked registers, IRQ dispatch and DAC samples. A separate legacy positive
control reconstructs only the pinned SDK's busy/deadline loop. With a busy
endpoint, the advancing-clock control times out after20300 modeled polls; the
frozen-clock control reaches its100000-poll watchdog without returning. This
demonstrates a conditional deadlock mechanism, not FM1's original trigger or
execution of its complete SDK driver.

Two600-second shared-controller runs pass. Normal capture receives600001 audio
packets and60000 CDC packets with no FIFO errors. Fault injection receives
574252 audio packets and59485 CDC packets, completing60000 service checks with
13 stream restart/reset operations,12 ready/busy races and16 stale CDC attempts.
It includes a5-second audio-only pause, a5-second whole-bus pause, periodic
350ms audio pauses and epoch rollover. Audio pauses leave serial progressing;
both endpoints recover after whole-bus backpressure. The deliberate host pauses
produce1107398 capture FIFO dropped frames, with zero capture underruns or
clipped samples. This is expected data loss under a stopped consumer, not a
spontaneous or persistent stall. The modeled timeout clock freezes for5 seconds
without affecting the new packet helper, which does not use that clock.

A separate1800-second Ray Force integrated musical replay produces1800001 USB
packets and79379968 DAC frames, with zero synth missing samples, rebuffers,
modeled FIFO errors, player errors or LCD errors. That replay still uses a
complete-packet SDK boundary; the shared endpoint harness uses a deterministic
test waveform instead of the sequencer. Neither executes Windows/OBS, DMA bus
arbitration, hardware register acknowledgement timing or the real scheduler.
The observed75-second hardware stall has not been reproduced spontaneously.

All12 CTests,7 client checks,4 screenshot checks, the descriptor check and10
linked corruption checks pass. Both board profiles relink with unchanged
application bytes. Prepared99bb92c full image eee4038f18ddc1e38a68ef4ed3b2b818d3fdc50e2a5d597edce945e3f1f39a8e
is unchanged and remains unflashed; its private original package and rollback
are preserved. This follow-up adds only host coverage and documentation.

### 2026-10-01 USB candidate installation and Ray Force bench

The user requested installing the connected device. Candidate99bb92c, full image
eee4038f18ddc1e38a68ef4ed3b2b818d3fdc50e2a5d597edce945e3f1f39a8e,
was written through the existing protected updater. All47 application sectors
and one full1MiB readback match. Boot/configuration protection remains intact;
c9810a47 remains the private rollback. No replacement bootloader was installed.

One reset was issued. The helper's known UTF8 log-decoding error was resolved
by successful CDC boot observation without repeating reset. The protected
session is idle/unblocked, with the new image as verified baseline and no
reset/observation pending. Demo playback progressed with zero underruns.

A6-second48kHz stereo16-bit WinMM input recording contains288000 frames and
nonzero audio. RAYFOR1.MDX and its named RAYFOR.PDX then uploaded successfully:
194663 bundle bytes,117.922 seconds, CRC/offset acknowledgements accepted.
PLAY succeeded, running/ready and zero synth/peripheral errors. Uploaded songs
remain volatile. Panel/encoder controls were used during subsequent playback.

Reopened capture completed89.97 seconds /4318560 frames of Ray Force, exceeding
the earlier75.62-second stall point. Peak10402, zero clipped samples. Concurrent
and post-capture CDC diagnostics respond: zero synth underruns/rebuffers/late
callbacks, bad USB packets, capture underruns and capture overflows. This is
one bounded recording, not complete-song or repeated OBS/reconnect acceptance.

Meter/full-view refresh remains below its20Hz target: later live readings are
7.2,7.6 and9.6FPS, following15.6-19.0 readings near transfer completion. No LCD
error is reported. This is a separate display-performance limitation and does
not establish MMDSP-equivalent redraw frequency. Internal audible output and
reference-motion acceptance remain unverified.

## 2026-10-02 MMDSP cadence and30FPS LCD candidate

The meter envelopes now use nominal55.45Hz fractional ticks, separately from
30Hz LCD requests. Default MMDSP velocity traces, fractional cadence, irregular
updates, frozen snapshots, dirty-row identity and packed RGB444 row order pass.
The single owner task still checks1470 queued audio frames before every LCD
write. Current USB packet/lifecycle fixes are retained.

Validation:15 CTests,7 client,4 screenshot and1 descriptor checks pass. Stock
12MHz/RGB565 and selected30MHz/RGB444 both link with the pinned SDK and pass
all11 linked corruption checks each. Reviewed LTO address changes add only
specific boot-trace/power operand encodings; the original normalized instruction
checks remain enforced, including a new boot-trace store corruption test.

Before correction, a30-second demo host replay completed19.93FPS. The initial
uncapped correction completed54.63FPS under the same400us/565-row wire model.
The final requested30FPS/30MHz/RGB444 Ray Force replay runs180 seconds at+500ppm:
5400 completed views (30.00FPS),180001 USB packets,7941952 DAC frames,9980 meter
ticks, zero synth missing frames, rebuffers, USB FIFO errors and peripheral
errors. Both rise/fall activity counters advance on all16 Ray Force parts.
The model validates actual task logic, row addresses, encoded pixel/sample
bytes and immutable completed views; it excludes physical CPU/DMA/USB timing.

Read-only pre-flash COM5 commands succeed. Installed firmware reports10.3FPS,
playback running, no synth/peripheral errors and zero synth underruns. Both
USB audio alternate settings are inactive at this observation; the output FIFO
has15 historical underruns, which is not a current capture error or a claim
that all USB faults are permanently resolved.

The protected session90d6c25107054d04bae2fa98e893fd4e is idle/unblocked before
installation. It accepts the offline application-only plan, directory sector
last; boot/config/reserved regions are preserved. The user explicitly approved
this exact candidate and requested RAYFOR1.MDX/RAYFOR.PDX upload/play afterward.

- Selected application:192208 bytes, SHA256
  `74f8a4f2d6cf1e14750db8e1c42c863178f513eedb78d77826fca4bfb5885d13`.
- Selected full-image SHA256:
  `4059dbd24bbe3d80794152a0140ab83cf1cc50fdcb15fff3052b9740b73b4f44`.
- Verified installed rollback SHA256:
  `eee4038f18ddc1e38a68ef4ed3b2b818d3fdc50e2a5d597edce945e3f1f39a8e`.

Hardware acceptance is separate from the host/model results above. See the
following deployment entry for the actual flash/boot/upload outcome.
