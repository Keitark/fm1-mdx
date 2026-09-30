# MMDSP-style MDX display

The240x240 LCD now has32 note-spectrum columns and16 part columns in one
horizontal row (FM1..8, PCM1..8). Both logical256x56 plots are uniformly scaled
to224x49, preserving their aspect ratio and28 vertical steps. L/R meters are
removed. Physical addressing remains rows0..239. The violet palette, karaoke
highlight and SELECT/FX/PLAY-STOP controls remain. Full part velocity uses27
steps, leaving the original top headroom within the28-step plot.

## Reference review and motion

The Ray Force reference is https://www.youtube.com/watch?v=UmHjuRGaTlU
(the supplied link label also pointed to GQ4tdCpw_ls). Frame comparisons at
38.41/38.56/38.86/39.36 seconds showed tall striped parts, configured-velocity
lines and independently falling spectrum caps. Source review:

- https://github.com/gaolay/MMDSP/blob/master/src/_LEVEL.s
- https://github.com/gaolay/MMDSP/blob/master/src/_SPEANA.s
- https://github.com/gaolay/MMDSP/blob/master/src/MXCTRL.s
- https://github.com/gaolay/MMDSP/blob/master/src/INIT.s

The standard MMDSP spectrum is derived from pitch/velocity events, rather than
an audio FFT. The FM1 implementation uses original C state machines for the
source's standard/integrating display rules and mathematical level tables:

- Part note-on resets the bar to max((velocity>>2)-4,0). Its thin line shows
  configured velocity, not a historical peak. A height-dependent counter with
  default sensitivity60 reduces held notes one step at a time. Note-off reduces
  a step every other logical tick. Short applied notes are latched between LCD
  updates. Manual karaoke notes are included; suppressed song triggers are not.
- Spectrum uses three semitones per column. Each onset adds linear velocity to
  its center and weighted contributions through five neighbors on either side;
  the distance4 shoulder is intentional. The standard level thresholds and
  half-level integration energy determine the new target. Rise traverses half
  the remaining distance, rounded up, per logical tick. Default decay uses the
  height-dependent counter with sensitivity24.
- Spectrum maximum lines hold60 logical ticks after a strictly higher level,
  then fall every6 ticks while the bar is nonzero, or every2 ticks when zero.
  Equal/lower arrivals do not restart the hold. Part velocity lines do not fall
  with their activity pulse. STOP clears both plots and their lines.

Logical ticks run at60Hz in the owner task, accumulating elapsed milliseconds;
LCD redraw is requested at20Hz. No new IRQ/timer is added. This reproduces the
source's counter rules, but does not establish exact timing of the supplied
X68000 video: the original vector/display mode and interrupt frequency are not
known. FM1 displays completed snapshots at its achievable LCD rate. This is not
a pixel-identical full MMDSP screen or a measured audio spectrum. The original
assembly is retained only as a private reference, not distributed in this repo.

The device no longer collects FFT windows or computes per-sample FM/PCM/LR
peaks. Existing FFT helpers remain for standalone host checks. Audio sample
clock, synthesis, PCM interpolation, mixing and USB output are unchanged. The
internal speaker/headphone path has no added LPF; the user chose to keep it
unfiltered. USB audio still precedes the physical-volume gain ramp.

## Embedded credits

The title parser retains explicitly embedded wording such asLAY0_V's
`Ar.By Veyrlen` andMASOSH's `by YURAYSAN`. It recognizes By/Ar.By/Arranged by/
Programmed by/Composed by without inventing an author role. When absent it shows
`NO EMBEDDED CREDIT`. Credits exceeding the224px text window scroll with end
pauses. Parsing is bounded to the first255 title bytes and127 credit characters;
non-ASCII bytes are shown as question marks by the current ASCII font. Metadata
is parsed once on load rather than on every refresh.

## Rendering and validation

Only rows changed from the last complete view are rendered/written. Row checks
compare displayed thresholds, cap positions and relevant text fields, without
hashes or a second rasterization. Four-row batches return to audio/control work;
LCD writes require at least1470 queued frames. The requested50ms view interval,
stock panel/pins/clock and synchronous DMA with IRQs enabled remain unchanged.
No full pixel framebuffer is added. Screenshot snapshots remain immutable and
match completed LCD views, including credit-scroll position.

Host regressions check source-derived counter traces, logical-tick invariance
at20FPS/12FPS/jittered updates, pitch spreading and saturation, peak behavior,
short triggers, karaoke ownership, PCM stop, metadata and all32/16 plot columns.
Dirty-row mutations include credit scrolling and spectrum caps. Every completed
replay view is compared against modeled physical LCD RAM at zero row offset.
The10-second demo WAV is byte-identical before/after these changes:
SHA256 b55f9ef31d89665937ef51e52185a7c44d949a8b5f59c59cbce7a6a46bee47db.

The180-second privateLAY0_V replay at+500ppm completes3598 modeled views
(19.99FPS),127262 row writes and736258 skipped rows, with zero missing audio
frames, rebuffers or USB FIFO errors. All14 parts used by this song show repeated
rises/falls; the remaining two PCM parts are idle. This400us/row wire-cost model
excludes PI32 execution cost, DMA behavior and the Windows driver. It is not a
hardware FPS or USB reliability result.

`python firmware/mdx/usb_client.py display --port COM4` reports completed-frame
count, FPS times10, written/skipped rows and maximum frame time. An unchanged
view still counts as a completed refresh. These are software completion counts,
not LCD scanout; the SDK clock has10ms resolution.

Build with `python scripts/build.py host` and
`python scripts/build.py firmware --usb-audio`; run the linked corruption checks
with `python firmware/usb-audio/test_link.py`. A private native-renderer preview
can be generated with `screen_preview song.MDX bank.PDX preview.i4` and converted
using `screenshot_png(data,4)`. Songs and generated audio remain private.

The revised motion/layout candidate awaits exact-image flashing and hardware
meter/FPS/control/audio acceptance. It does not resolve the independent
capture-active USB audio byte-alignment/CDC stall investigation under#9.

## Historical bench evidence

## First bench result and correction

Image00032c4e9fc259e400808023161ebb9b8f7e0eeb8304ec7864ee128b47ce649f
was explicitly approved, written and fully readback-verified. It boots, but
measured refresh is6.7-6.8FPS and `keys=-3` persists after a cold power cycle.
The existing scanner-failure path stops the ADC and clears its volume target,
muting internal audio while USB capture remains audible before that envelope.
This image is not accepted for panel/internal-audio use.

The correction supplies the scanner with the SDK clock's10000us quantum; its
no-progress limit becomes10000us plus one quantum. With a quantized clock,
observations1us apart can otherwise appear10000us apart and falsely trip the
watchdog. The actual driver regression reproduces that boundary, and still
rejects a real no-progress interval. Default high-resolution users retain the
original limit. Volume ADC sampling now has its own lifetime on the shared
1ms timer; scanner failure still releases manual notes and disables matrix
scanning, but leaves the independent, bounded ADC volume service running.
ADC failure and final teardown still fail muted.

Row rendering skips irrelevant spectrum/part regions and removes repeated
formatting of16 static labels on every row. A measured song preview remains
pixel-identical. New `MDX VOLUME` and `MDX SCAN` commands expose volume target,
ADC progress and scanner failure snapshots. Correction host tests pass; hardware FPS, controls and audio still require
bench validation. The user subsequently requested flashing after investigation.


## Panel addressing follow-up

The panel-init table sets rows40..279, but the stock fill and qualified NES
async path override it with a zero-based window. The MDX writer mistakenly
reintroduced y+40. It now writes rows0..239. Target-boundary tests assert the
actual RASET commands for all240 rows; replay maps those physical addresses
directly to the expected image, without compensating for an offset in the mock.

After0284ee8, live diagnostics measured20FPS, keys=0, valid/progressing ADC
and no reported synth underruns/rebuffering. The50ms view interval (20FPS) and per-row audio-reserve gate remain unchanged
at the user's request. Meter movement still requires bench observation. USB error31
recurred during a Laydock upload; that transport fault is still unresolved.

## 2026-10-01 elapsed-time meter correction

Commit27f6c39 adds the MMDSP-inspired envelopes described above. All9 CTests,
7 client checks,4 screenshot checks, the descriptor test and5 linked corruption
checks pass. The192528-byte application links with the pinned SDK and audits.
A180-second private Laydock replay at+500ppm reports zero missing frames,
rebuffers and USB FIFO errors;3598 modeled views,71642 written and791878 skipped
rows. This is a host wire-cost model, not a hardware FPS or Windows USB test.

Image2065596f902a109bf054ac0aca46068a3b53051f0e16829b2e755c5ef242c4c9
was installed in48 directory-last sectors and the full1MiB readback matches.
The confirmed zero-row-offset imageca04b4e9... is preserved privately for rollback.
A single reset and successful CDC boot observation show advancing audio frames
with zero reported underruns/peripheral errors. Physical meter timing acceptance
and the independent capture-active serial/USB noise investigation remain open.

The capture-closed reload ofLAY0_V.MDX andLAY_V.PDX completed in88.1s and
PLAY was acknowledged. Two live readings show ready/running, frames advancing,
keys=0, valid ADC, zero reported synth underruns/rebuffers/peripheral errors and
14.5-17.8FPS. Scanner completions continue; USB capture submissions are zero.
This verifies playback/CDC progress with capture closed, not the unresolved
simultaneous capture/CDC case or the subjective reference-meter match.
