"""Offline regression checks against the linked composite ELF; no device I/O."""
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'firmware/nes'))
from audit_boot import audit, PARTS


class LinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = ROOT / 'build/target-audio'
        cls.elf = (cls.folder / 'fm1-mdx.elf').read_bytes()
        offset = struct.unpack_from('<I', cls.elf, 32)[0]
        count, strings = struct.unpack_from('<HH', cls.elf, 48)
        raw = [struct.unpack_from('<10I', cls.elf, offset + i * 40) for i in range(count)]
        s = raw[strings]
        names = cls.elf[s[4]:s[4] + s[5]]
        cls.sections = {names[s[0]:names.index(b'\0', s[0])].decode(): s for s in raw}
        cls.symbols = {}
        for line in (cls.folder / 'symbols.txt').read_text().splitlines():
            fields = line.split()
            if len(fields) == 4:
                cls.symbols[fields[3]] = int(fields[0], 16)

    def check_elf(self, elf):
        app = b''.join(elf[self.sections[n][4]:self.sections[n][4] + self.sections[n][5]] for n in PARTS)
        return audit(elf, app, usb_only=True, usb_peripheral_tests=True,
                     usb_mdx=True, usb_audio=True, require_boot_trace=True,
                     require_board_power=True)

    def corrupt(self, symbol, delta):
        address = self.symbols[symbol] + delta
        section = next(s for s in self.sections.values()
                       if s[1] != 8 and s[3] <= address < s[3] + s[5])
        elf = bytearray(self.elf)
        elf[section[4] + address - section[3]] ^= 4
        return elf

    def test_original_passes(self):
        self.assertEqual(self.check_elf(self.elf)['static_audit'], 'passed')

    def test_descriptor_corruption_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'UAC1 interface/terminal'):
            self.check_elf(self.corrupt('fm1_uac_descriptor', 90))

    def test_power_destination_corruption_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Power merged-global target changed: sys_low_power'):
            self.check_elf(self.corrupt('power_init', 0x3c2))

    def test_power_gateway_corruption_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Board power gateway instructions changed'):
            self.check_elf(self.corrupt('fm1_board_power_init', 24))


if __name__ == '__main__':
    unittest.main()
