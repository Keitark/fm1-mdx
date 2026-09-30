# Source and license provenance

- RetroFM bounded MDX sequencer, PDX parser and PCM mixer are copied from the
  local RetroFM checkout at `3aad4b3d893dcb11cf6318bd8fee18c9022eccd5`.
  Their GPL-3.0-or-later notices and `MDX_PORT_NOTES.md` remain in
  `firmware/mdx/vendor/retrofm`.
- The adapted mdxtools FM driver and original C YM2151 emulator come from
  [vampirefrog/mdxtools](https://github.com/vampirefrog/mdxtools), revision
  `606e3a7009aa1a9dfa6bee8bc875dbd5483714e9`. Package GPLv3 text and original
  authorship notices are preserved in `firmware/mdx/vendor/mdxtools`.
  The YM2151 source credits Jarek Burczynski and Tatsuyuki Satoh.
- Shared FM1 board-support/CDC/audit code is adapted from the FM1 research and
  FM1-NES source workspace. Existing dependency notices remain applicable;
  the Apache-2.0 license text is in `licenses/Apache-2.0.txt`.
- The AC79 SDK and Jieli toolchain are external build dependencies. No SDK
  library or toolchain executable is bundled. See `FM1_THIRD_PARTY.md` for the
  original FM1 source preparation's dependency and board-data disclosures.
- The LCD initialization table, key mapping, GPIO/power settings and volume
  input have stock-analysis provenance. This is not a clean-room claim. The
  inherited `FM1_PROVENANCE.md` and `FM1_LCD_PROVENANCE.md` retain that context;
  their paths describe the parent FM1 preparation, not additional bundled files.
- `FM1DEMO.MDX` and `FM1DEMO.PDX` are generated from the original generator
  in `firmware/mdx/samples/make_demo.py`; no commercial music or sample bank is
  used.

The top-level GPL license applies to original MDX additions. Third-party source
retains its own notices. Any later binary distribution must account for all
linked SDK/library terms and corresponding-source obligations.
