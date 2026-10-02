# FM1 MDX karaoke player

An experimental MDX/PDX player for the M-VAVE FM-1. It uses the display, DAC,
keyboard, encoders and USB CDC support developed for FM1-NES, with a bounded
RetroFM MDX sequencer and software YM2151 synthesis.

**Status: flashed on the FM-1; screen and demo audio confirmed on2026-09-30.**
Full flash readback matched. Live CDC upload/playback and karaoke control smoke
checks passed with zero audio underruns. Detailed physical karaoke and stereo
qualification remain pending; see [VALIDATION.md](VALIDATION.md).
This repository contains source and original demo assets. It contains no stock
firmware, device backups, chip keys, commercial songs, vendor SDK libraries or
unit-specific flash images.

## Playback and karaoke

- Stereo YM2151 at 44.1kHz, plus PDX ADPCM/PCM8 through the RetroFM mixer.
- Original `FM1DEMO.MDX` and `FM1DEMO.PDX` are compiled into read-only flash.
- The demo starts with its repeating PCM drum on track9 muted. Uploaded songs
  start with their original mix; track mute controls remain available.
- Choose an FM track, enable karaoke mute and play its current voice with the
  physical keyboard. This is monophonic on the chosen YM2151 channel.
- Muting releases the existing song note once. Further playback note-ons **and
  note-offs** are suppressed; voice, volume, pan and modulation updates continue.
- Manual pitch replaces the song's base pitch while retaining its software
  detune, portamento and LFO offset. Patch changes can therefore change a held
  manual note. Unmuting releases the manual note and resumes at the next song
  trigger; it does not retrigger an old note.
- PCM tracks can be muted too. Manual keyboard replacement currently covers FM
  tracks1–8; PCM sample audition is not implemented.

## Controls

OCT−/OCT+ shift new keyboard notes by an octave, bounded to−3..+2. A held note
keeps its original pitch until released. FX toggles karaoke mute; PLAY/STOP
toggles playback. The SELECT knob beside master volume selects an FM track
(encoder0). The selector correction combines two decoded contact edges into
one track step, retaining partial/reversed movement and fast turns. Physical
detent behavior of this new correction is awaiting a bench check.

When the selected FM part is in karaoke mute, turning SELECT restores that
part's song note-on/off and meter activity from its next score note, and moves
karaoke mute to the newly selected part. Its patch parameters continue updating.
Other independently muted parts keep their settings. Selecting the same part
leaves a held manual note untouched. With karaoke off, selection stays unmuted.
The selection-follow correction is installed; physical meter acceptance is pending.

SEL toggles **timed guide mode** (installed; physical LED acceptance pending). Enabling
it mutes the selected FM part's song triggers and lights its next keyboard note.
OCT-/OCT+ also lights when an octave shift is required. The footer shows the
next note and octave direction. A matching key-down immediately shows the next
candidate; a wrong note or key-up does not advance it. If you miss the note, the
guide moves on at its scheduled DAC time. Accompaniment keeps playing normally.
Guide-off leaves karaoke mute as set; use FX to unmute. Guidance follows the
karaoke part when SELECT moves its mute. Stop clears the lights; restarting
with guide enabled mutes the initially selected FM part again. Guidance covers
FM1-8, not PCM sample keys; the physical keyboard range is MIDI17-103.
The installed combined correction blanks LEDs during the paced scan idle and
lights the final row during the next existing transfer. All rows receive one
modeled transfer interval instead of the final row staying lit through idle.
Input pacing and SPI transfer count are unchanged; physical brightness still
requires a user check.
The installed brightness correction extends these equal pulses using SPI2 divider119
while guide LEDs are requested (four times divider29's modeled transfer duration).
The divider changes only between complete sweeps and returns to29 when LEDs clear.
The existing1ms timer and11 transfers remain; GPIO drive settings are unchanged.
`scan` reports `baud_written` as software intent because BAUD is write-only.
Full flash readback is verified; live scanning retains approximately1kHz sweep
pacing with brighter timing active and no reported faults. Physical brightness
and key/audio acceptance are pending.
USB equivalents: `guide --enable 1`, `guide --enable 0`, or `guide` for status,
using `python firmware/mdx/usb_client.py ... --port <FM1 CDC port>`.

Slots14–40 are the keyboard, MIDI53–79 before octave shifting. The panel order
is slots0/1 for OCT−/OCT+,2..7 for FX/SEL/ENV/LFO/EDIT/GLO, and8..13 for
HOME/SAVE/ARP/SEQ/PLAY-STOP/REC. FX/SEL are user-observed; the remaining names
follow the supplied panel order and require the corrected candidate's bench test.
`MDX STATUS` exposes `oct`, held `panel` bits and the `last` pressed panel slot
for verification. Encoder direction also remains to be confirmed.
`python firmware/mdx/usb_client.py input --port COM10` also reads all seven
encoder counters so an unexpected physical mapping can be identified.
The volume knob uses the recovered PB6 ADC input, starting muted and ramping.

## Build and test

For host checks on Windows, install Python3, CMake and Visual Studio2022 with
C++ tools. No SDK, device or Python third-party package is required for tests.

```powershell
python scripts/build.py host
```

To link the FM-1 application, supply the external AC79 SDK at revision
`e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d` and an existing Jieli pi32v2 toolchain:

```powershell
$env:FM1_SDK_DIR = 'C:\path\to\fw-AC79_AIoT_SDK'
$env:FM1_TOOLCHAIN_DIR = 'C:\JL\pi32\bin'
python scripts/build.py firmware
```

SDK upstream: [Jieli AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK).
Without `FM1_SDK_DIR`, the default is `.deps/ac79-sdk`. The checkout must be clean
and match the pin. The builder calls compiler/linker tools directly; it never
runs the SDK's Makefile or download scripts.

`build/target` contains an ELF, application payload, map, disassembly, source
hashes and static audit. The application payload is **not a flash/update file**.
The audit retains the SDK startup/integrity checks and IRQ-based UBOOT recovery.
There is no firmware flasher or stock-layout packager in this repository.

An optional composite USB Audio Class1 + CDC profile adds computer playback
through FM1 and MDX recording to the computer at48kHz stereo16-bit PCM:

```powershell
python scripts/build.py firmware --usb-audio
```

The separately built candidate is in `build/target-audio`. Host and link checks
pass. The USB packet/lifecycle candidate is now flashed with bounded MDX-to-PC
capture checks; complete Windows/OBS and analog qualification remain open. See
[USB_AUDIO.md](USB_AUDIO.md) for routing, conversion and bench checks.

## USB song loading

After this candidate has been installed and its CDC port confirmed, install
`pyserial` in the PC Python environment. Use the actual COM port listed below:

```powershell
python firmware/mdx/usb_client.py list
python firmware/mdx/usb_client.py upload --port COM10 --mdx song.MDX --pdx song.PDX
python firmware/mdx/usb_client.py play --port COM10
python firmware/mdx/usb_client.py select --port COM10 --track 1
python firmware/mdx/usb_client.py mute --port COM10 --track 1 --on 1
python firmware/mdx/usb_client.py status --port COM10
```

COM10 is an example. The client checks VID/PID and the MDX firmware identity.
PDX must accompany an MDX that names a sample bank. One bundle fits192KiB,
including its12-byte header. Upload stops playback, checks ordered offsets,
CRC and file structure, then stores the song in RAM. Uploaded files disappear
on power-off. The built-in flash demo remains available with `demo`.

CDC song loading is implemented; USB mass-storage drag-and-drop and persistent
uploaded-song storage are not implemented. USB MIDI and TRS/BLE MIDI input are
also outside this first candidate. MIDI-note commands are available over CDC.

## Screen capture

The pending MMDSP-style display adds32 note-spectrum columns with peak lines,
16 inline FM8/PCM8 activity meters, embedded credits and a20Hz redraw target
with audio-reserve checks. L/R meters are removed; internal audio stays unfiltered. See[DISPLAY.md](DISPLAY.md)
for meter definitions, measured host results and the hardware acceptance boundary.

The new candidate includes a CDC screenshot command:

```powershell
python firmware/mdx/usb_client.py screenshot --port COM10 --output fm1-screen.png
```

It saves the last completed240x240 MDX UI frame as a CRC-checked PNG while
playback continues. See[SCREENSHOT.md](SCREENSHOT.md) for capture semantics.
This feature is installed on FM1 and has transferred a completed screen while
OutRun USB recording continued with zero reported underruns. See[VALIDATION.md](VALIDATION.md).

## Layout and provenance

`firmware/mdx` holds the player, original sample generator, USB client and tests.
`firmware/usb-diag` holds shared CDC recovery services. `firmware/nes` holds the
reused board-support and audit files only; no NES core or game is bundled.

Original project additions are GPL-3.0-or-later; dependency notices are retained.
See [THIRD_PARTY.md](THIRD_PARTY.md) and [VALIDATION.md](VALIDATION.md).
This is independent experimental firmware, with stock-analysis provenance, and
is not an official M-VAVE or Jieli product.
