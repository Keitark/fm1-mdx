# FM1 MDX karaoke player

An experimental MDX/PDX player for the M-VAVE FM-1. It uses the display, DAC,
keyboard, encoders and USB CDC support developed for FM1-NES, with a bounded
RetroFM MDX sequencer and software YM2151 synthesis.

**Status: host-tested candidate. Not yet installed or accepted on the FM-1.**
This repository contains source and original demo assets. It contains no stock
firmware, device backups, chip keys, commercial songs, vendor SDK libraries or
unit-specific flash images.

## Playback and karaoke

- Stereo YM2151 at 44.1kHz, plus PDX ADPCM/PCM8 through the RetroFM mixer.
- Original `FM1DEMO.MDX` and `FM1DEMO.PDX` are compiled into read-only flash.
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

Scanner slots0/1 select the previous/next FM track, slot2 toggles karaoke mute,
and slot3 toggles play/stop. Encoder0 also selects a track. Slots14–40 are the
keyboard, MIDI53–79. These are stock-derived scanner assignments; physical
button names, encoder direction and behavior require this candidate's bench test.
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

## Layout and provenance

`firmware/mdx` holds the player, original sample generator, USB client and tests.
`firmware/usb-diag` holds shared CDC recovery services. `firmware/nes` holds the
reused board-support and audit files only; no NES core or game is bundled.

Original project additions are GPL-3.0-or-later; dependency notices are retained.
See [THIRD_PARTY.md](THIRD_PARTY.md) and [VALIDATION.md](VALIDATION.md).
This is independent experimental firmware, with stock-analysis provenance, and
is not an official M-VAVE or Jieli product.
