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
This describes class compatibility; enumeration of this firmware is untested.
There are no advertised device-volume or sample-rate controls to implement.

The DAC and MDX synth remain44.1kHz. Two bounded1024-frame stereo queues and
linear interpolation convert between the DAC clock and USB48kHz. A bounded
queue-fill correction handles clock drift. Startup priming adds several
milliseconds of buffering; actual end-to-end latency and conversion quality
are not measured. This initial profile does not claim professional converter
performance. There is still one synth task and one analog audio owner.

The MDX recording source is declared as the embedded Synthesizer terminal
type0x0713. Windows maps that defined type to its synthesizer pin category;
the original undefined input terminal failed to produce a recording endpoint
on the first device test. See[Microsoft's pin mapping](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/pin-category-property).

Read streaming counters after installation using the actual CDC port:

```powershell
python firmware/mdx/usb_client.py audio --port COM10
```

`MDX AUDIO` reports active directions, packet/error counts, queue fill and
underrun/overrun counters. Reset/disconnect clears active streams and queued
PCM. UBOOT teardown disarms USB audio and rejects later stream activation.

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

Before hardware acceptance, install one reviewed candidate with the known
working CDC firmware rollback ready. Confirm separate CDC and USB audio
functions, left/right playback, MDX recording with PC return excluded, PC audio
while MDX is stopped, concurrent CDC uploads/control, sustained duplex counters,
disconnect/reconnect and UBOOT recovery. The profile has not been flashed yet.
