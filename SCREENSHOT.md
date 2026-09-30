# CDC screenshots

After installing a firmware candidate that contains the screenshot feature,
save the displayed MDX UI using the actual CDC port:

```powershell
python firmware/mdx/usb_client.py screenshot --port COM10 --output fm1-screen.png
```

Without `--output`, the client saves a timestamped PNG in the current directory.
It needs the existing `pyserial` dependency; PNG encoding uses Python's standard
library. The screenshot is a240x240 image of the MDX UI viewport, whose LCD RAM
row offset is40. It captures the last completed UI frame using the same font,
colors and pixel renderer used to write the LCD. This is a software screenshot;
it does not read pixels back from the LCD controller or show physical panel
faults. A partly updated LCD may be ahead of the captured completed frame.

The USB task freezes240 bytes of frame text/state, then regenerates its indexed
pixels during transfer. The displayed UI can keep updating. No full framebuffer
is allocated, playback is not stopped, and CRC generation yields to other tasks
every eight rows. Actual capture-time audio underruns still need a bench check.
Capture requires one completed LCD frame and rejects a known LCD error.

The protocol is:

- `MDX SHOT BEGIN`: freeze a completed frame and return its transfer token,
  dimensions, packed2-bit format, CRC32 and displayed frame number.
- `MDX SHOT READ <8-hex-token> <8-hex-byte-offset>`: return at most96 indexed
  pixel bytes as hex. Offsets are aligned to96 and bounded to14,400 bytes.
- `MDX SHOT END <8-hex-token>`: release the frozen transfer.

A new capture invalidates the old token. Disconnect/reset clears the transfer;
30 seconds without a read expires it. Each reply fits the existing bounded CDC
line framing. The PC checks tokens, offsets, length, palette and whole-frame CRC
before writing a PNG; malformed or incomplete transfers produce no output file.

Host tests compare every screenshot pixel with LCD row output, freeze state
while the live UI changes, and check timeout/token/bounds behavior. The PC test
transfers through the real fragmented CDC parser, validates PNG chunks and
rejects corrupted transfers. Device screenshot matching remains unverified.
