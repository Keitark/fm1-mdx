"""Validate explicitly pinned local MDX/PDX bytes and generate private C arrays.

No asset discovery, network access, playback, SDK invocation, or device access.
Generated song bytes belong only in a fresh ignored/private build directory.
Header/chunk checks follow the existing RetroFM front end; full sequencing and
physical playback remain separate acceptance checks.
"""

from dataclasses import dataclass
import hashlib
from pathlib import Path
import re
import struct


HASH_PATTERN = re.compile(r"[0-9a-f]{64}\Z")
MAX_ASSET_BYTES = 0x100000  # Even the complete physical FM-1 image is only 1 MiB.
PDX_TABLE_BYTES = 96 * 8


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _text(raw, label, multiline=False):
    _require(bool(raw), label + ' is empty')
    _require(all(value >= 32 or (multiline and value in (9, 10, 13))
                 for value in raw), label + ' contains control bytes')
    try:
        text = raw.decode('cp932', errors='strict')
    except UnicodeDecodeError:
        raise ValueError(label + ' is not valid CP932') from None
    _require(bool(text.strip()), label + ' is empty')
    return text


def validate_mdx(data, pdx_filename):
    """Validate title/reference and the 9/16-track, 27-byte voice chunk layout."""
    _require(8 <= len(data) <= MAX_ASSET_BYTES, 'MDX size is empty or outside the bound')
    end = data.find(b'\r\n\x1a')
    _require(end >= 0, 'MDX title terminator is absent')
    title = _text(data[:end], 'MDX title', multiline=True)
    name_start = end + 3
    name_end = data.find(b'\0', name_start)
    _require(name_end >= 0, 'MDX PDX reference is unterminated')
    name = _text(data[name_start:name_end], 'MDX PDX reference')
    _require(not any(character in name for character in '/\\:') and
             name not in ('.', '..') and name == name.strip(),
             'MDX PDX reference must be a local filename')
    reference = name[:-4] if name.lower().endswith('.pdx') else name
    _require(bool(reference) and reference not in ('.', '..'), 'MDX PDX reference is empty')
    filename = Path(pdx_filename).name
    supplied = filename[:-4] if filename.lower().endswith('.pdx') else filename
    _require(reference.casefold() == supplied.casefold(), 'MDX PDX reference does not match the selected PDX filename')
    data_start = name_end + 1
    _require(data_start + 7 <= len(data), 'MDX offset header is truncated')
    _require(data[data_start + 4:data_start + 7] != b'LZX', 'Compressed LZX MDX is unsupported')
    first_track = struct.unpack_from('>H', data, data_start + 2)[0]
    _require(first_track in (20, 34), 'MDX must have a 9- or 16-track offset table')
    tracks = first_track // 2 - 1
    _require(data_start + first_track <= len(data), 'MDX offset table is truncated')
    offsets = struct.unpack_from('>' + str(tracks + 1) + 'H', data, data_start)
    starts = [data_start + offset for offset in offsets]
    _require(all(offset >= first_track and start < len(data)
                 for offset, start in zip(offsets, starts)), 'MDX chunk offset is outside the file')
    _require(len(set(starts)) == len(starts), 'MDX chunks overlap')
    voice_start = starts[0]
    voice_end = min([start for start in starts if start > voice_start] + [len(data)])
    voice_bytes = voice_end - voice_start
    _require(voice_bytes > 0 and voice_bytes % 27 == 0 and voice_bytes // 27 <= 256,
             'MDX voice chunk has an invalid size')
    numbers = data[voice_start:voice_end:27]
    _require(len(set(numbers)) == len(numbers), 'MDX voice numbers are duplicated')
    return {'title': title, 'pdx_reference': name, 'tracks': tracks,
            'voice_count': voice_bytes // 27}


def validate_pdx(data):
    """Use the same 96 big-endian offset/length entries as retrofm_pdx_open."""
    _require(PDX_TABLE_BYTES <= len(data) <= MAX_ASSET_BYTES,
             'PDX table is empty, truncated, or outside the bound')
    nonempty = 0
    for index in range(96):
        offset, length = struct.unpack_from('>II', data, index * 8)
        if length:
            _require(offset >= PDX_TABLE_BYTES and offset <= len(data) and
                     length <= len(data) - offset,
                     'PDX sample table contains an out-of-bounds range')
            nonempty += 1
    return {'samples': nonempty}


@dataclass(frozen=True)
class PrivateSong:
    mdx_path: Path
    pdx_path: Path
    mdx: bytes
    pdx: bytes
    metadata: dict

    def verify_sources(self):
        for label, path, expected in (
            ('MDX', self.mdx_path, self.mdx), ('PDX', self.pdx_path, self.pdx)
        ):
            _require(path.stat().st_size == len(expected) and
                     sha256(path.read_bytes()) == sha256(expected),
                     'Private ' + label + ' input changed during the build')


def select_private_song(mdx=None, pdx=None, mdx_sha256=None, pdx_sha256=None):
    selected = (mdx, pdx, mdx_sha256, pdx_sha256)
    if all(value is None for value in selected):
        return None
    _require(all(value is not None for value in selected),
             'Private default requires both MDX/PDX paths and both pinned SHA256 values')
    for expected in (mdx_sha256, pdx_sha256):
        _require(isinstance(expected, str) and HASH_PATTERN.fullmatch(expected) is not None,
                 'Private asset SHA256 must be 64 lowercase hexadecimal characters')
    paths = (Path(mdx).resolve(strict=True), Path(pdx).resolve(strict=True))
    payloads = []
    for label, path, expected in zip(('MDX', 'PDX'), paths, (mdx_sha256, pdx_sha256)):
        _require(path.is_file() and 0 < path.stat().st_size <= MAX_ASSET_BYTES,
                 'Private ' + label + ' input is not a bounded nonempty file')
        data = path.read_bytes()
        _require(0 < len(data) <= MAX_ASSET_BYTES and sha256(data) == expected,
                 'Private ' + label + ' SHA256 or size mismatch')
        payloads.append(data)
    _require(sum(map(len, payloads)) <= MAX_ASSET_BYTES,
             'Private assets together exceed the complete physical image size')
    mdx_info = validate_mdx(payloads[0], paths[1].name)
    pdx_info = validate_pdx(payloads[1])
    metadata = {'kind': 'private_song', **mdx_info, 'pdx_samples': pdx_info['samples']}
    for key, path, data in zip(('mdx', 'pdx'), paths, payloads):
        metadata[key] = {'size': len(data), 'sha256': sha256(data),
                         'source_path_sha256': sha256(str(path).encode('utf-8'))}
    return PrivateSong(paths[0], paths[1], payloads[0], payloads[1], metadata)


def _require_private_output(song, out, source_root):
    out = Path(out).resolve()
    source_root = Path(source_root).resolve()
    _require(not out.exists(), 'Private builds require a NEW output path; existing outputs are never overwritten')
    for assets in (song.mdx_path.parent, song.pdx_path.parent,
                   source_root / 'firmware/mdx/samples'):
        _require(not out.is_relative_to(assets.resolve()), 'Private output is inside a source asset directory')
    if out.is_relative_to(source_root):
        _require(out.relative_to(source_root).parts[0] in ('build', 'tmp', 'local'),
                 'Private output inside the checkout must use ignored build/, tmp/, or local/')
    return out


def generate_private_song(song, out, source_root):
    out = _require_private_output(song, out, source_root)
    # mkdir is exclusive as well: a competing builder cannot win the check/write
    # race and have its artifacts or completed manifest overwritten.
    out.mkdir(parents=True, exist_ok=False)
    generated = out / 'generated'
    generated.mkdir()
    source = generated / 'private-song.c'
    lines = ['/* Private generated input: do not commit or copy into samples/. */',
             '#include <stddef.h>']
    for name, data in (('mdx', song.mdx), ('pdx', song.pdx)):
        lines.append('const unsigned char fm1_demo_' + name + '[] = {')
        for offset in range(0, len(data), 16):
            lines.append('    ' + ','.join('0x%02x' % value for value in data[offset:offset + 16]) + ',')
        lines.append('};')
        lines.append('const size_t fm1_demo_' + name + '_size = sizeof(fm1_demo_' + name + ');')
    with source.open('x', encoding='ascii', newline='\n') as stream:
        stream.write('\n'.join(lines) + '\n')
    return source
