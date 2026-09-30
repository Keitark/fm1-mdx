# Validation record

Date:2026-09-30 (Asia/Tokyo).

## Host checks

`python scripts/build.py host` passed in the standalone repository:

- Two native CTest groups: player/karaoke/USB contracts and FM1 task lifecycle.
- Four Python integration checks using the real C CDC line parser, with
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

- Confirm this exact image boots, shows the menu and enumerates CDC.
- Confirm physical button/encoder assignments and keyboard pitch.
- Observe DAC format/clocks and audition low-gain stereo output.
- Run a sustained demo and record underruns; the host does not measure the
  target's synthesis throughput or display/USB scheduling margins.
- Hold a live note through playback rests and patch changes while muted.
- Load a real MDX with its PDX over USB, play it and verify disconnect behavior.
- Verify STOP and serial UBOOT entry on hardware with the known rollback ready.

No device flash, reset, port opening or live hardware read occurred in this run.
Persistent uploaded songs, drag-and-drop mass storage and MIDI input remain
unimplemented. PCM playback uses bounded48kHz-to44.1kHz sample resampling;
high-quality PDX interpolation and broad MDX compatibility need further tests.
