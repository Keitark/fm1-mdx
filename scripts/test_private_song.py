"""Offline private-default contracts using only original/synthetic demo bytes."""
import hashlib
import importlib.util
from pathlib import Path
import re
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import private_song as song
import build as build_wrapper

spec = importlib.util.spec_from_file_location(
    'original_demo_fixture', HERE.parent / 'firmware/mdx/samples/make_demo.py')
demo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(demo)


class PrivateSongTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / 'checkout'
        self.root.mkdir()
        self.assets = self.base / 'assets'
        self.assets.mkdir()
        self.mdx_path = self.assets / 'original.mdx'
        self.pdx_path = self.assets / 'FM1DEMO.PDX'
        self.mdx, self.pdx = demo.make()
        self.mdx_path.write_bytes(self.mdx)
        self.pdx_path.write_bytes(self.pdx)
        self.mdx_hash = song.sha256(self.mdx)
        self.pdx_hash = song.sha256(self.pdx)

    def selected(self):
        return song.select_private_song(self.mdx_path, self.pdx_path,
                                        self.mdx_hash, self.pdx_hash)

    def test_no_inputs_retains_original_demo_selection(self):
        self.assertIsNone(song.select_private_song())

    def test_every_partial_combination_is_rejected(self):
        complete = [self.mdx_path, self.pdx_path, self.mdx_hash, self.pdx_hash]
        for mask in range(1, 15):
            with self.subTest(mask=mask):
                args = [value if mask & (1 << index) else None
                        for index, value in enumerate(complete)]
                with self.assertRaisesRegex(ValueError, 'requires both'):
                    song.select_private_song(*args)

    def test_metadata_has_exact_hashes_sizes_and_opaque_provenance(self):
        selected = self.selected()
        self.assertEqual(selected.metadata['kind'], 'private_song')
        self.assertEqual(selected.metadata['title'], 'FM1 original karaoke demo')
        self.assertEqual(selected.metadata['tracks'], 9)
        for key, path, data in (('mdx', self.mdx_path, self.mdx),
                                ('pdx', self.pdx_path, self.pdx)):
            self.assertEqual(selected.metadata[key]['sha256'], song.sha256(data))
            self.assertEqual(selected.metadata[key]['size'], len(data))
            self.assertEqual(selected.metadata[key]['source_path_sha256'],
                             hashlib.sha256(str(path.resolve()).encode()).hexdigest())
            self.assertNotIn(str(path), str(selected.metadata))

    def test_cp932_title_is_preserved_as_unicode(self):
        title = 'オリジナルのテスト曲'
        mdx = title.encode('cp932') + self.mdx[self.mdx.index(b'\r\n\x1a'):]
        self.assertEqual(song.validate_mdx(mdx, self.pdx_path.name)['title'], title)

    def test_generated_arrays_roundtrip_exact_bytes_and_are_const(self):
        selected = self.selected()
        source = song.generate_private_song(selected, self.root / 'build/private-one', self.root)
        raw = source.read_text()
        for label, expected in (('mdx', self.mdx), ('pdx', self.pdx)):
            match = re.search(r'const unsigned char fm1_demo_' + label + r'\[\] = \{(.*?)\};', raw, re.S)
            self.assertIsNotNone(match)
            restored = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', match.group(1)))
            self.assertEqual(restored, expected)
            self.assertIn('const size_t fm1_demo_' + label + '_size = sizeof(fm1_demo_' + label + ');', raw)
        self.assertEqual(source.name, 'private-song.c')
        self.assertEqual(source.parent.name, 'generated')

    def test_completed_or_empty_existing_output_is_never_overwritten(self):
        out = self.root / 'build/existing'
        out.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, 'NEW output'):
            song.generate_private_song(self.selected(), out, self.root)
        marker = out / 'build-manifest.json'
        marker.write_text('completed evidence')
        with self.assertRaisesRegex(ValueError, 'NEW output'):
            song.generate_private_song(self.selected(), out, self.root)
        self.assertEqual(marker.read_text(), 'completed evidence')

    def test_outputs_inside_asset_or_public_source_directories_are_rejected(self):
        for out in (self.assets / 'new-build',
                    self.root / 'firmware/mdx/samples/private-build',
                    self.root / 'scripts/private-build'):
            with self.subTest(path=out):
                with self.assertRaises(ValueError):
                    song.generate_private_song(self.selected(), out, self.root)
                self.assertFalse(out.exists())

    def test_modified_mdx_hash_is_rejected_before_generation(self):
        self.mdx_path.write_bytes(self.mdx + b'\0')
        with self.assertRaisesRegex(ValueError, 'MDX SHA256'):
            self.selected()

    def test_modified_pdx_hash_is_rejected_before_generation(self):
        self.pdx_path.write_bytes(self.pdx + b'\0')
        with self.assertRaisesRegex(ValueError, 'PDX SHA256'):
            self.selected()

    def test_changed_input_during_build_is_rejected(self):
        selected = self.selected()
        selected.verify_sources()
        self.pdx_path.write_bytes(self.pdx[:-1] + b'\0')
        with self.assertRaisesRegex(ValueError, 'changed during'):
            selected.verify_sources()

    def test_malformed_or_missing_hashes_are_rejected(self):
        for digest in ('', '0' * 63, 'x' * 64, self.mdx_hash.upper(), 0):
            with self.subTest(digest=digest):
                with self.assertRaises(ValueError):
                    song.select_private_song(self.mdx_path, self.pdx_path, digest, self.pdx_hash)

    def test_empty_files_are_rejected(self):
        for path in (self.mdx_path, self.pdx_path):
            with self.subTest(path=path):
                old = path.read_bytes()
                path.write_bytes(b'')
                with self.assertRaisesRegex(ValueError, 'nonempty file'):
                    self.selected()
                path.write_bytes(old)

    def test_mdx_title_and_reference_errors_are_rejected(self):
        tail = self.mdx[self.mdx.index(b'\0', self.mdx.index(b'\x1a') + 1) + 1:]
        for raw in (b'no terminator', b'\r\n\x1aFM1DEMO\0' + tail,
                    b' \r\n\x1aFM1DEMO\0' + tail,
                    b'bad\0title\r\n\x1aFM1DEMO\0' + tail,
                    b'\x81\r\n\x1aFM1DEMO\0' + tail,
                    b'title\r\n\x1aunterminated',
                    b'title\r\n\x1a\0' + tail,
                    b'title\r\n\x1a../FM1DEMO\0' + tail,
                    b'title\r\n\x1aFM1DEMO\r\0' + tail):
            with self.subTest(raw=raw[:30]):
                with self.assertRaises(ValueError):
                    song.validate_mdx(raw, self.pdx_path.name)

    def test_pdx_name_mismatch_is_rejected_and_case_extension_are_accepted(self):
        with self.assertRaisesRegex(ValueError, 'does not match'):
            song.validate_mdx(self.mdx, 'another.PDX')
        self.assertEqual(song.validate_mdx(self.mdx, 'fm1demo.pdx')['tracks'], 9)
        extended = self.mdx.replace(b'FM1DEMO\0', b'FM1DEMO.PDX\0', 1)
        self.assertEqual(song.validate_mdx(extended, 'fm1demo.pdx')['tracks'], 9)

    def test_invalid_mdx_offset_and_voice_chunks_are_rejected(self):
        data_start = self.mdx.index(b'\0', self.mdx.index(b'\x1a') + 1) + 1
        for index, value in ((2, 18), (0, 0xffff), (0, 20)):
            raw = bytearray(self.mdx)
            struct.pack_into('>H', raw, data_start + index, value)
            with self.subTest(index=index, value=value):
                with self.assertRaises(ValueError):
                    song.validate_mdx(bytes(raw), self.pdx_path.name)
        with self.assertRaisesRegex(ValueError, 'voice chunk'):
            song.validate_mdx(self.mdx[:-1], self.pdx_path.name)

    def test_pdx_sample_bounds_cover_the_complete_table(self):
        for slot, offset, size in ((0, 1, 2), (0, len(self.pdx), 1),
                                  (0, 768, 0xffffffff), (95, len(self.pdx) + 1, 2)):
            raw = bytearray(self.pdx)
            struct.pack_into('>II', raw, slot * 8, offset, size)
            with self.subTest(slot=slot, offset=offset, size=size):
                with self.assertRaisesRegex(ValueError, 'out-of-bounds'):
                    song.validate_pdx(bytes(raw))
        with self.assertRaisesRegex(ValueError, 'truncated'):
            song.validate_pdx(b'\0' * 767)

    def test_firmware_wrapper_forwards_all_private_flags(self):
        argv = ['build.py', 'firmware', '--out', 'build/new-private',
                '--default-mdx', 'local/original.mdx', '--default-pdx', 'local/FM1DEMO.PDX',
                '--default-mdx-sha256', self.mdx_hash, '--default-pdx-sha256', self.pdx_hash]
        with patch.object(sys, 'argv', argv), patch.object(build_wrapper, 'run') as runner:
            build_wrapper.main()
        passed = list(map(str, runner.call_args.args[0]))
        for name in ('--out', '--default-mdx', '--default-pdx',
                     '--default-mdx-sha256', '--default-pdx-sha256'):
            self.assertIn(name, passed)
        self.assertIn(self.mdx_hash, passed)
        self.assertIn(self.pdx_hash, passed)

    def test_host_wrapper_rejects_private_flags_without_running_build(self):
        with patch.object(sys, 'argv', ['build.py', 'host', '--default-mdx', 'local/original.mdx']), \
                patch.object(build_wrapper, 'run') as runner, patch('sys.stderr'):
            with self.assertRaises(SystemExit):
                build_wrapper.main()
        runner.assert_not_called()


if __name__ == '__main__':
    unittest.main()
