# Optional USB audio profile

Build with `python scripts/build.py firmware --usb-audio` after configuring the
SDK/toolchain variables in the README. This produces a separate candidate in
`build/target-audio`. The default command still builds CDC-only firmware in
`build/target`. Installing either candidate uses the separate FM1 bench workflow.

The composite profile has three functions on one USB cable:

- CDC: the existing MDX/PDX RAM loader, player controls, diagnostics and guarded
  UBOOT recovery, on interfaces0/1.
- USB playback: stereo48kHz16-bit PCM from the computer, mixed with MDX audio
  through the existing FM1 DAC. It also works while MDX playback is stopped.
- USB recording: stereo48kHz16-bit PCM of the MDX synth, including live keyboard
  karaoke, sent to the computer. It taps before the analog master-volume ramp
  and excludes computer playback to avoid a digital return loop.

Recording does not read an analog input. The recording level follows the MDX
mix; the master-volume knob controls the analog output. Full-scale PC audio and
MDX audio together can clip at the saturating mixer. Adjust the PC playback
level when combining them.

Audio Class1 uses interfaces2..4 and endpoint1 in both directions. CDC uses
endpoint2 OUT, endpoint2 IN for notifications and endpoint3 IN for data.
Each audio direction transfers192 bytes every1ms; duplex PCM payload totals
384kB/s, within USB full-speed's nominal12Mb/s capacity. Real scheduling and
CPU margins still require measurements on the device.

The device advertises an IAD composite identity (`EF/02/01`) and fixed48kHz
stereo16-bit PCM. Windows includes a native [USB Audio Class1 driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/usb-audio-class-system-driver--usbaudio-sys-)
and supports [IAD function grouping](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-interface-association-descriptor).
The first hardware installation enumerated CDC and playback; recording needed
the compatibility correction described below.
There are no advertised device-volume or sample-rate controls to implement.

The DAC and MDX synth remain44.1kHz. Two bounded1024-frame stereo queues and
linear interpolation convert between the DAC clock and USB48kHz. A bounded
queue-fill correction handles clock drift. Startup priming adds several
milliseconds of buffering; actual end-to-end latency and conversion quality
are not measured. This initial profile does not claim professional converter
performance. There is still one synth task and one analog audio owner.

The MDX recording source uses virtual Line Connector terminal type0x0603 for
Windows recording compatibility. Both the original undefined input and the
embedded Synthesizer type0x0713 failed to produce a recording endpoint on this
Windows machine. With0x0713, a direct KS query confirmed the stereo48kHz capture
pin and synthesizer bridge existed, but PnP and WinMM still exposed only playback.
The line classification describes the virtual recording path; it adds no analog
ADC or input jack. See[Microsoft's pin mapping](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/pin-category-property).

Read streaming counters after installation using the actual CDC port:

```powershell
python firmware/mdx/usb_client.py audio --port COM10
```

`MDX AUDIO` reports active directions, packet/error counts, queue fill and
underrun/overrun counters. Reset/disconnect clears active streams and queued
PCM. UBOOT teardown disarms USB audio and rejects later stream activation.
PC playback has a32-frame start/recovery/release ramp (about0.73ms at the DAC)
to avoid a full-scale step when an application stops supplying packets. This
does not alter the MDX recording tap.

The packet submission candidate replaces normal audio-IN and CDC-IN calls to
`usb_g_iso_write`/`usb_g_bulk_write` with `fm1_usb_packet_write`. The pinned SDK's
shared `usb_g_ep_write` polls TxPktRdy using a jiffies deadline; these two
application paths no longer enter it. USB0 EP1 accepts exactly192 bytes; CDC
EP3 accepts1..63 bytes. A busy, unconfigured, missing-DMA or stale-session
submission returns zero without changing DMA bytes or the packet count register.

The helper copies into the already configured DMA buffer, writes the exact
USB0 TXCNT and performs a memory synchronization before setting TxPktRdy. It
holds the SDK's counted IRQ mask inside a saved caller IRQ state, so nested
register helpers cannot unmask interrupts before commit. This is one bounded
packet attempt, not a general replacement for SDK USB control/RX handling;
SDK register access still uses its finite hardware-acknowledgement waits.

The composite build binds the USB control task and the peripheral/synth owner
to CPU0 using the pinned SDK's `#C0` task-table prefix. USB and ALINK interrupts
already use CPU0. This keeps packet commit and stream teardown on the same core;
the CPU-local IRQ guard would not serialize a task running on CPU1. The linked
audit verifies both task-table names. This consolidates task load on CPU0, so
real render reserve and screen FPS must be checked after installation.

Capture uses a separate192-byte CPU staging packet. If submission is rejected,
it retains that packet for retry rather than consuming another48 source frames.
Every capture stop/start/reset invalidates the epoch and clears staging, so an
old pending packet cannot enter a new stream. An early busy check also avoids
unnecessary encoding. Stream setup masks completion interrupts, flushes and
configures DMA, primes capture, then enables the callback.
CDC submissions carry the reply queue's session generation, preventing a reset
during command processing or formatting from sending an old reply after reconnect.

`python firmware/mdx/usb_client.py usb --port COM4` reports accepted submissions,
rejected/short submissions, busy callbacks, start/stop counts, last CSR/write
result, capture epoch and pending byte count. Existing `tx` counts encoder
attempts, not successful transport. Acceptance by the helper is not proof of
bytes received by Windows. Tests cover100000 mixed audio/CDC submissions,
busy and stale-session cancellation, DMA headroom, nested IRQ restoration,
pending retry and restart discard.

On installed c9810a47 firmware, the Ray Force full-recording attempt stopped
receiving USB audio and CDC heartbeats after75.62 captured seconds; COM4 then
failed Windows configuration with error31. The partial WAV peak was10395 with
zero clipping. The exact halt cause has not been established. This new packet
path is an unflashed candidate and does not yet establish that the stall or OBS
mute/unmute corruption is fixed. Full-song and repeated restart bench checks
remain required.

`python firmware/mdx/usb_client.py timing --port COM4` reports coarse DAC
callback spacing, callbacks separated by at least20ms, minimum primed queue
fill, rebuffer events/missing frames and longest render duration. Linked SDK
disassembly shows `timer_get_ms` reads `jiffies*10`: resolution is10ms. A normal
callback crossing a tick reads10ms. The clock cannot certify the1.45ms DMA deadline; these are diagnostic
counters, not proof that every hardware DMA completion was serviced on time.

## Verification

```powershell
python scripts/build.py host
python scripts/build.py firmware
python scripts/build.py firmware --usb-audio
python firmware/usb-audio/test_link.py -v
```

The host checks cover actual descriptors and SDK callback adapter logic,
malformed requests/packets, stereo isolation, clipping, reset/UBOOT teardown
and60 simulated seconds at0 and±500ppm DAC drift. The final command checks the
linked composite ELF and rejects descriptor and power-instruction corruption.

`usb_stall_tests` in the host Release build executes the actual UAC target and
packet helper against a shared endpoint model for600 virtual seconds. Run it
without arguments for continuous capture/CDC, or with `stress` for audio-only
and whole-bus host pauses, ready/busy races, stale serial sessions, epoch rollover
and stream resets. Audio-only backpressure must leave serial progressing;
both endpoints must recover after whole-bus backpressure ends. Deliberately
stopping host reads can drop capture samples; it must not cause a permanent CPU
stall. CTest bounds both runs with a20-second watchdog.

A separate positive control reconstructs only the reviewed pinned SDK writer's
busy/deadline loop: TxPktRdy stays set, neither escape flag is set, and jiffies
either advances or is deliberately frozen. It times out with the advancing
clock; with the frozen clock it reaches the100000-iteration watchdog. That is
a conditional deadlock demonstration, not execution of the SDK binary or a
spontaneous reproduction of FM1's failure. The model does not emulate register
acknowledgement delays, DMA bus arbitration, actual IRQ priorities, Windows or
OBS. Normal OBS software mute is not modeled as a device stream transition.

Before hardware acceptance, install one reviewed candidate with the known
working CDC firmware rollback ready. Confirm separate CDC and USB audio
functions, left/right playback, MDX recording with PC return excluded, PC audio
while MDX is stopped, concurrent CDC uploads/control, sustained duplex counters,
disconnect/reconnect and UBOOT recovery. See[VALIDATION.md](VALIDATION.md) for
the installed revisions and remaining hardware checks.
