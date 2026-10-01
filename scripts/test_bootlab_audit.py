"""Host audit-contract tests; optionally exercise a real offline linked ELF."""
import argparse
from pathlib import Path
import struct
import unittest
from unittest.mock import patch

import audit_bootlab as lab


def synthetic_model():
    # Invented ELF model for audit rules, not an emulated CPU or ROM layout.
    base = lab.RAM_BEGIN
    values = {"__lab_ram_begin": base, "__lab_ram_end": lab.RAM_END,
              "_start": base, "lab_entry_c": base + 28, "lab_clear_bytes": base + 32,
              "lab_synthetic_check": base + 36, "fm1_boot_crc16": base + 40,
              "fm1_boot_layout_valid": base + 44, "fm1_boot_validate": base + 48,
              "__lab_bss_begin": base + 64, "__lab_bss_end": base + 96,
              "lab_result": base + 64, "lab_finished": base + 68,
              "__lab_sp_begin": base + 96, "__lab_sp_top": base + 2144,
              "__lab_ssp_begin": base + 2144, "__lab_ssp_top": base + 4192,
              "__lab_used_end": base + 4192}
    symbols = {k: dict(value=v, size=4 if k in ("lab_result", "lab_finished") else 0)
               for k, v in values.items()}
    code = (b"\xc0\xff" + struct.pack("<I", values["__lab_sp_top"]) + b"\x64\xe0\x80\x0e" +
            b"\xc0\xff" + struct.pack("<I", values["__lab_ssp_top"]) + b"\x64\xe0\x80\x0d" +
            b"\x80\xea\x02\x00\x00\x00\xf7\x9e")
    specs = [(".start", 1, 6, 0, 28, code), (".text", 1, 6, 28, 36, b"\0" * 36),
             (".bss", 8, 3, 64, 32, b""), (".stacks", 8, 3, 96, 4096, b"")]
    sections = [dict(name=n, type=t, flags=f, address=base + at, size=size,
                     contents=c, offset=at, align=1) for n, t, f, at, size, c in specs]
    segment = dict(address=base, file_offset=0, file_bytes=64, memory_bytes=4192)
    # Small invented disassembly sufficient to exercise ordering rules.
    dis = "_start:\n"
    ops = ["r0 = 1", "sp = r0", "r0 = 2", "ssp = r0",
           "call 4 <lab_entry_c : 1c0201c >", "nop", "goto -4 <_start+0x18 : 1c02018 >"]
    dis += "".join(f"  {base + i * 2:x}:    00 00 \t{op}\n" for i, op in enumerate(ops))
    dis += "\nlab_entry_c:\n  1c0201c:    00 00 \tcall 1 <lab_clear_bytes : 1c02020 >\n"
    dis += "  1c0201e:    00 00 \tcall 1 <lab_synthetic_check : 1c02024 >\n"
    dis += "\nlab_clear_bytes:\n  1c02020:    00 00 \trts\n"
    return (base, sections, symbols, [segment]), dis


class LayoutTests(unittest.TestCase):
    def setUp(self):
        self.model, self.dis = synthetic_model()

    def check_model(self, model=None, dis=None):
        with patch.object(lab, "parse_elf", return_value=model or self.model):
            return lab.audit(b"unused", self.dis if dis is None else dis)

    def test_valid_synthetic_layout(self):
        report = self.check_model()
        self.assertFalse(report["execution_performed"])
        self.assertFalse(report["rom_abi_verified"])
        self.assertEqual(report["stacks_bytes"], 4096)

    def test_wrong_entry(self):
        with self.assertRaisesRegex(ValueError, "entry differs"):
            self.check_model((self.model[0] + 2, *self.model[1:]))

    def test_unexpected_external_symbol(self):
        self.model[2]["unknown_rom_function"] = dict(value=0x1000, size=0)
        with self.assertRaisesRegex(ValueError, "global symbols"):
            self.check_model()

    def test_overlap(self):
        self.model[1][-1]["address"] -= 32
        with self.assertRaisesRegex(ValueError, "overlapping"):
            self.check_model()

    def test_bss_has_file_bytes(self):
        self.model[1][2]["type"] = 1
        with self.assertRaises(ValueError):
            self.check_model()

    def test_bad_stack_immediate(self):
        self.model[1][0]["contents"] = b"\xc0\xff\0\0\0\0" + self.model[1][0]["contents"][6:]
        with self.assertRaisesRegex(ValueError, "stack immediates"):
            self.check_model()

    def test_wrong_stack_bounds(self):
        self.model[2]["__lab_ssp_begin"]["value"] -= 32
        with self.assertRaisesRegex(ValueError, "stack bounds"):
            self.check_model()

    def test_wrong_section_permissions(self):
        self.model[1][1]["flags"] = 3
        with self.assertRaisesRegex(ValueError, "permission"):
            self.check_model()

    def test_missing_load_coverage(self):
        self.model[3][0]["memory_bytes"] = 64
        with self.assertRaisesRegex(ValueError, "covered"):
            self.check_model()

    def test_clear_after_check(self):
        swapped = self.dis.replace("lab_clear_bytes :", "TEMP :").replace(
            "lab_synthetic_check :", "lab_clear_bytes :").replace("TEMP :", "lab_synthetic_check :")
        with self.assertRaisesRegex(ValueError, "clear BSS before"):
            self.check_model(dis=swapped)

    def test_invalid_elf_headers(self):
        for data in (b"", b"\x7fELF", b"\x7fELF\x02\x01\x01" + b"\0" * 100):
            with self.assertRaises(ValueError):
                lab.parse_elf(data)


class LinkedElfTests(unittest.TestCase):
    """Only enabled with explicit target outputs; no hardware is invoked."""
    def test_actual_link_and_mutations(self):
        data = ELF_PATH.read_bytes()
        dis = DIS_PATH.read_text(encoding="utf-8")
        lab.audit(data, dis)
        _, sections, _, _ = lab.parse_elf(data)
        start = next(s for s in sections if s["name"] == ".start")
        shoff = struct.unpack_from("<I", data, 32)[0]
        phoff = struct.unpack_from("<I", data, 28)[0]
        bss_i = next(i for i, s in enumerate(sections) if s["name"] == ".bss")
        symtab = next(s for s in sections if s["type"] == 2)
        # Find a named global definition and turn it into an unresolved symbol.
        sym_offset = next(at for at in range(symtab["offset"], symtab["offset"] + symtab["size"], 16)
                          if data[at + 12] >> 4 and struct.unpack_from("<I", data, at)[0])
        mutations = [(24, struct.pack("<I", lab.RAM_BEGIN + 2)),
                     (18, b"\0\0"),
                     (phoff + 20, struct.pack("<I", 0xffffffff)),
                     (shoff + bss_i * 40 + 4, struct.pack("<I", 1)),
                     (start["offset"] + 2, b"\0\0\0\0"),
                     (start["offset"] + 24, b"\xff\xff"),
                     (sym_offset + 14, b"\0\0"),
                     (32, struct.pack("<I", len(data) + 1))]
        for offset, replacement in mutations:
            broken = bytearray(data)
            broken[offset:offset + len(replacement)] = replacement
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                lab.audit(bytes(broken), dis)
        with self.assertRaises(ValueError):
            lab.audit(data[:-16], dis)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--disassembly", type=Path)
    args = parser.parse_args()
    if bool(args.elf) != bool(args.disassembly):
        parser.error("both target output paths are required")
    ELF_PATH, DIS_PATH = args.elf, args.disassembly
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(LayoutTests)
    if ELF_PATH:
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(LinkedElfTests))
    raise SystemExit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
