"""Regression checks for bank validation, never against a device."""
import struct
import unittest

from study_bootloader import BASE, crc16, decode_sdk_bank, inspect_boot


def bank(code=bytes.fromhex('d8 e8 07 00 c0 ff') + bytes(26), **kwargs):
    fields = dict(count=1, load=BASE, offset=16)
    fields.update(kwargs)
    head = struct.pack('<HHIIH', fields['count'], len(code), fields['load'],
                       fields['offset'], crc16(code))
    return head + struct.pack('<H', crc16(head)) + code


class BankTests(unittest.TestCase):
    def test_crc_known_vector(self):
        self.assertEqual(crc16(b'123456789'), 0x31c3)

    def test_valid_bank(self):
        code, result = decode_sdk_bank(bank())
        self.assertEqual(len(code), 32)
        self.assertTrue(result['header_and_data_crc_valid'])

    def test_corrupt_header(self):
        data = bytearray(bank()); data[14] ^= 1
        with self.assertRaisesRegex(ValueError, 'CRC mismatch'):
            decode_sdk_bank(data)

    def test_corrupt_payload(self):
        data = bytearray(bank()); data[-1] ^= 1
        with self.assertRaisesRegex(ValueError, 'CRC mismatch'):
            decode_sdk_bank(data)

    def test_truncation(self):
        for data in (bank()[:12], bank()[:-1]):
            with self.assertRaises(ValueError):
                decode_sdk_bank(data)

    def test_wrong_layout(self):
        for data in (bank(count=2), bank(load=BASE+4), bank(offset=32), bank()+b'\0'):
            with self.assertRaisesRegex(ValueError, 'layout'):
                decode_sdk_bank(data)

    def test_non_instruction_payload(self):
        with self.assertRaisesRegex(ValueError, 'startup'):
            decode_sdk_bank(bank(bytes(32)))

    def test_unreviewed_handoff(self):
        for sdk in (False, True):
            with self.assertRaisesRegex(ValueError, 'Unreviewed boot'):
                inspect_boot(bytes(16000), sdk)


if __name__ == '__main__':
    unittest.main()
