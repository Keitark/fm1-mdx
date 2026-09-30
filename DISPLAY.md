# Pocket-style MDX display

The 240x240 UI uses a dark violet palette, two song-title lines, a stereo FFT,
L/R output peaks, eight FM/MDX meters and eight PCM8 meters. The selected FM
part has a frame; karaoke parts use amber. The footer names the physical
SELECT knob, FX mute and PLAY/STOP controls. OCT-/OCT+ retain keyboard transpose.

The spectrum is a256-point radix-2 fixed-point FFT of a complete stereo audio
window, with a periodic Hann window. Each of24 bands takes the maximum bin
power across both channels, so opposite-phase signals do not cancel. Resolution
is172.27Hz; the final band includes Nyquist at22.05kHz. Levels map approximately
-60..0dBFS to bar height. The input is the final synthesized mix before the
analog volume envelope; PC recording and meters therefore do not follow the
physical volume knob. FFT windows are sampled once per UI update, not continuously.

Part meters accumulate actual sample peaks between updates. FM taps each OPM
carrier sum after FINAL_SH, before the shared cubic output curve and group gain.
PCM taps decoded/interpolated voice samples after voice gain/pan, before the
final PCM group gain and stereo clipping. These are voice activity/peak meters,
not isolated FFTs, calibrated loudness comparisons or note-on indicators. Manual
karaoke notes are included. Decaying bars and slower peak caps aid reading.

The owner task requests a new view every50ms (20Hz target). It compares rows
against the last completed view and writes only changed rows. At most four rows
are examined per batch, then audio synthesis/control service resumes. Audio ring
occupancy must be at least1470 frames before each LCD row. Synchronous stock DMA
waits keep IRQs enabled; LCD pins, clock and controller setup are unchanged.
The old10ms sleep after every row is removed. No full-frame pixel buffer is added.

The latest candidate checks row-specific visual dependencies before rendering.
Static rows require no rasterization. FFT/bar segments compare their displayed
thresholds; peak markers compare positions; text compares relevant fields. A
changed row is rendered once, replacing old/new rasterization and pixel comparison.
The dependency check is conservative, without hashes. Regression mutations
check that every skipped row has identical pixels; replay checks completed LCD
RAM against the full renderer. Hardware FPS remains to be measured.

For the updated renderer, the same180s Super Laydock replay at+500ppm completes
3598 views with85117 written/778403 skipped rows and zero audio errors. The
slightly conservative dependencies write about2.4% more rows than exact pixel
comparison in that case, while eliminating the second render and all renders
of unchanged rows. Host wire timing still excludes PI32 execution costs.

`python firmware/mdx/usb_client.py display --port COM4` returns completed-frame
count, `fps10` (ten times FPS, over the last reporting interval), written/skipped
rows, maximum frame-completion time and the20Hz target. An unchanged view still
counts as a completed refresh; use row counters to distinguish physical writes.
Long or audio-constrained frames can run below target. These counters measure
software completion, not LCD scanout, and the SDK clock has10ms resolution.

## Validation and bench boundary

The host replay models400us per LCD pixel-row transfer, the real10ms RTOS tick,
audio consumer and UAC encoder. Normal private Super Laydock playback at+500ppm
for180 simulated seconds completed3598 views (19.99FPS),83094 row writes and
780426 skipped rows, with no synth missing frames, rebuffers or USB FIFO errors.
The initial full redraw took90ms in that model. The model excludes actual PI32
CPU/render costs, DMA behavior and the Windows driver; hardware FPS is unverified.

Regression tests check FFT frequencies, silence, full scale and opposite-phase
stereo, screenshot pixel/palette/CRC agreement, frozen captures and token bounds.
The replay checks every committed view against modeled LCD RAM, including dirty
rows. A10-second demo WAV remains byte-identical before/after meter instrumentation.
The board link retains startup, power and recovery audits; exact reviewed LTO
global relocations are checked, including corruption rejection tests.

Build with `python scripts/build.py host` and
`python scripts/build.py firmware --usb-audio`. Generate a native-renderer preview
with `screen_preview song.MDX bank.PDX preview.i4`; the PC helper
`screenshot_png(data,4)` converts it to PNG without changing palette colors.
Commercial inputs and generated recordings stay outside this repository.

This UI candidate requires an exact-image approved flash, actual FPS/audio tests,
karaoke/control acceptance, and a live screenshot check. It does not resolve or
qualify the separately investigated USB audio byte-alignment noise.


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
