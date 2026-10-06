# Source publication record

Reviewed 2026-10-07 for [issue #20](https://github.com/Keitark/fm1-mdx/issues/20).
This is a bounded source and repository review, not a legal opinion or a claim
that the firmware was developed in a clean room.

## License and attribution

- The top-level `LICENSE` is GPL-3.0. Original MDX player additions are offered
  under GPL-3.0-or-later to the extent their authors hold the rights.
- The vendored RetroFM MDX/PCM source carries GPL-3.0-or-later notices. The
  vendored mdxtools components are from the GPLv3 package at pinned revision
  `606e3a7009aa1a9dfa6bee8bc875dbd5483714e9`; the YM2151 header retains
  Jarek Burczynski's authorship credit. See [THIRD_PARTY.md](THIRD_PARTY.md)
  and the license files within each vendor directory.
- The inherited FM1 board support and build overlays were adapted from the
  FM1-NES project. Its Apache-2.0 upstream inputs retain their attribution and
  license notices. Apache-2.0 code may be combined into a GPLv3 work while its
  own notices and terms remain applicable; this does not change third-party
  ownership. See [GNU's compatibility guidance](https://www.gnu.org/licenses/license-compatibility.html)
  and [FM1_THIRD_PARTY.md](FM1_THIRD_PARTY.md).
- The external Jieli SDK and pi32v2 toolchain are required for a target build.
  Neither is bundled. The SDK repository-level Apache-2.0 notice does not
  establish redistribution rights for each precompiled archive or toolchain
  component. This repository publishes no linked firmware image.
- `FM1DEMO.MDX` and `FM1DEMO.PDX` exactly match the original
  `firmware/mdx/samples/make_demo.py` generator. No commercial song or sample
  bank is included.

## Board research retained in source

The display initialization table in `firmware/nes/boot/display_test.c`, the
boot handoff, keyboard mapping and power values were developed using analysis
of the stock FM-1 firmware and hardware. The table has 21 records and remains
attributed as stock-derived in [FM1_LCD_PROVENANCE.md](FM1_LCD_PROVENANCE.md).
Some entries match published controller examples; that does not change the
original research provenance. The table is published as source with this
disclosure. This review does not determine whether every board parameter has
copyright protection or whether the vendor grants separate permission for it.
An independently documented and device-tested replacement remains possible
future work. No claim of vendor endorsement is made.

## Repository history check

The local Git history reachable from all remote branches was checked before
changing visibility: 41 commits and 407 unique blobs, about 3.43 MB of blob
data. Path and content scans found no stock firmware image, unit-specific
backup, private key, token-like credential, SDK archive or commercial MDX/PDX.
The generated demo files were compared byte-for-byte with their generator.
GitHub had 11 branch refs, no releases or Actions artifacts, and draft feature
PRs. Issue and PR descriptions were also screened for credential patterns with
no matches. This is a
point-in-time check; future contributions require the same source-only rule.

The default `main` branch contains the initial player. The most recent karaoke
guide work remains in [draft PR #19](https://github.com/Keitark/fm1-mdx/pull/19)
and its branch until its physical acceptance is complete. Publication does not
change that PR's draft or validation status.

## Distribution boundary

Source publication does not include a firmware binary or imply that external
SDK archives, a stock ROM, commercial songs, or bundled software from unrelated
devices may be redistributed. A later binary release needs its own review of
the exact linked SDK libraries, third-party notices, corresponding source and
assets. The inherited Jieli USB identity also needs a distribution decision
before treating this as a consumer product.
