"""Strict ELF/layout and entry-disassembly inspection for the offline lab only.

This is not a CPU emulator, ROM-ABI proof or hardware validation. Uses only
stdlib and the linked lab ELF/disassembly; no vendor/stock analysis imports.
"""
import json
from pathlib import Path
import re
import struct

RAM_BEGIN, RAM_END = 0x1c02000, 0x1c22000
STACK_BYTES = 2048


def require(condition, message):
    if not condition:
        raise ValueError(message)


def parse_elf(data):
    """Read the small ELF32 little-endian subset emitted by our pinned tools."""
    def span(at, size):
        require(0 <= at <= len(data) and 0 <= size <= len(data) - at,
                "truncated ELF")
        return data[at:at + size]

    def string(table, offset):
        require(0 <= offset < len(table), "invalid string offset")
        end = table.find(b"\0", offset)
        require(end >= 0, "unterminated ELF string")
        return table[offset:end].decode("ascii")

    require(span(0, 7) == b"\x7fELF\x01\x01\x01", "expected ELF32 little endian")
    header = struct.unpack("<HHIIIIIHHHHHH", span(16, 36))
    kind, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phcount, shsize, shcount, shstr = header
    # 241 is the value emitted by this pi32v2 toolchain, not a portable
    # identifier establishing CPU compatibility for arbitrary ELF producers.
    require((kind, machine, version, flags, ehsize, shsize) == (2, 241, 1, 0, 52, 40),
            "unexpected ELF type/toolchain header")
    require(0 < shcount <= 128 and 0 < shstr < shcount, "invalid section table")
    raw = [struct.unpack("<10I", span(shoff + i * shsize, shsize))
           for i in range(shcount)]
    strings = span(raw[shstr][4], raw[shstr][5])
    sections = []
    for record in raw:
        name, typ, attrs, address, offset, size, link, info, align, entsize = record
        contents = b"" if typ == 8 else span(offset, size)
        sections.append(dict(name=string(strings, name), type=typ, flags=attrs,
                             address=address, offset=offset, size=size,
                             align=align, contents=contents))
    symbols = {}
    for record in raw:
        if record[1] != 2:
            continue
        require(record[9] == 16 and record[5] % 16 == 0 and record[6] < shcount,
                "invalid symbol table")
        table = span(raw[record[6]][4], raw[record[6]][5])
        for at in range(record[4], record[4] + record[5], 16):
            name, value, size, info, other, index = struct.unpack("<III BB H", span(at, 16))
            label = string(table, name)
            if not label:
                continue
            require(index != 0, "unresolved symbol: " + label)
            require(index < shcount or index == 0xfff1, "invalid symbol section: " + label)
            if info >> 4:  # Only named global/weak symbols are layout contracts.
                require(label not in symbols, "duplicate global symbol")
                symbols[label] = dict(value=value, size=size, section=index)
    require(phcount > 0 and phsize == 32, "invalid program header table")
    segments = []
    for i in range(phcount):
        typ, at, va, pa, filesz, memsz, attrs, align = struct.unpack(
            "<8I", span(phoff + i * phsize, phsize))
        require(typ == 1, "unexpected non-load segment")
        span(at, filesz)
        require(filesz <= memsz and va == pa and RAM_BEGIN <= va <= RAM_END
                and memsz <= RAM_END - va, "load segment outside lab RAM")
        segments.append(dict(address=va, file_offset=at, file_bytes=filesz, memory_bytes=memsz))
    return entry, sections, symbols, segments


def audit(data, disassembly):
    entry, sections, symbols, segments = parse_elf(data)

    def value(name):
        require(name in symbols, "missing symbol: " + name)
        return symbols[name]["value"]

    expected_symbols = {"_start", "lab_entry_c", "lab_clear_bytes", "lab_synthetic_check",
                        "lab_result", "lab_finished", "fm1_boot_crc16",
                        "fm1_boot_layout_valid", "fm1_boot_validate",
                        "__lab_ram_begin", "__lab_ram_end", "__lab_bss_begin", "__lab_bss_end",
                        "__lab_sp_begin", "__lab_sp_top", "__lab_ssp_begin", "__lab_ssp_top",
                        "__lab_used_end"}
    require(set(symbols) == expected_symbols, "unexpected or missing global symbols")

    require(value("__lab_ram_begin") == RAM_BEGIN and value("__lab_ram_end") == RAM_END,
            "RAM window differs from lab profile")
    require(entry == value("_start") == RAM_BEGIN, "entry differs from RAM origin")
    allowed = {".start", ".text", ".rodata", ".data", ".bss", ".stacks"}
    allocated = [s for s in sections if s["flags"] & 2]
    previous = RAM_BEGIN
    for s in sorted(allocated, key=lambda s: s["address"]):
        require(s["name"] in allowed, "unexpected allocated section: " + s["name"])
        require(previous <= s["address"] <= RAM_END and s["size"] <= RAM_END - s["address"],
                "overlapping or out-of-RAM section")
        previous = s["address"] + s["size"]
        require(s["type"] in (1, 8), "unexpected allocated section type")
        expected_flags = {".start": 6, ".text": 6, ".rodata": 2,
                          ".data": 3, ".bss": 3, ".stacks": 3}
        require(s["flags"] == expected_flags[s["name"]], "section permission mismatch")
        coverage = [p for p in segments if p["address"] <= s["address"] and
                    s["address"] + s["size"] <= p["address"] + p["memory_bytes"]]
        require(len(coverage) == 1, "section not covered by one RAM load segment")
        if s["type"] == 1:
            p = coverage[0]
            require(s["address"] + s["size"] <= p["address"] + p["file_bytes"] and
                    s["offset"] == p["file_offset"] + s["address"] - p["address"],
                    "section load bytes mismatch")
    by_name = {s["name"]: s for s in allocated}
    require(len(by_name) == len(allocated), "duplicate allocated section")
    for name in (".start", ".text", ".bss", ".stacks"):
        require(name in by_name and by_name[name]["size"] > 0, "missing section: " + name)
    bss, stacks, start = (by_name[n] for n in (".bss", ".stacks", ".start"))
    require(bss["type"] == stacks["type"] == 8, "BSS/stacks must be NOBITS")
    require(bss["address"] == value("__lab_bss_begin") and
            bss["address"] + bss["size"] == value("__lab_bss_end"), "BSS extent mismatch")
    require(value("__lab_bss_end") <= value("__lab_sp_begin"), "BSS overlaps SP stack")
    require(stacks["address"] == value("__lab_sp_begin") and stacks["size"] == 2 * STACK_BYTES,
            "stack section mismatch")
    require(value("__lab_sp_top") - value("__lab_sp_begin") == STACK_BYTES and
            value("__lab_ssp_begin") == value("__lab_sp_top") and
            value("__lab_ssp_top") - value("__lab_ssp_begin") == STACK_BYTES,
            "separate stack bounds mismatch")
    require(value("__lab_used_end") == value("__lab_ssp_top") == previous,
            "used RAM end mismatch")
    for name in ("__lab_bss_begin", "__lab_bss_end", "__lab_sp_begin",
                 "__lab_sp_top", "__lab_ssp_begin", "__lab_ssp_top"):
        require(value(name) % 32 == 0, "unaligned lab bound")
    for name in ("lab_result", "lab_finished"):
        require(value("__lab_bss_begin") <= value(name) and
                value(name) + symbols[name]["size"] <= value("__lab_bss_end") and
                symbols[name]["size"] == 4, "breadcrumb outside BSS")
    for name in ("lab_entry_c", "lab_clear_bytes", "lab_synthetic_check",
                 "fm1_boot_crc16", "fm1_boot_layout_valid", "fm1_boot_validate"):
        require(by_name[".text"]["address"] <= value(name) <
                by_name[".text"]["address"] + by_name[".text"]["size"],
                "function outside lab text: " + name)

    # Inspect entry bytes as well as decoded instruction order. The two MOV
    # immediates must use linked tops, preventing a pretty disassembly with
    # wrong stack addresses from passing the audit.
    code = start["contents"]
    require(len(code) == 28 and code[:2] == b"\xc0\xff" and code[10:12] == b"\xc0\xff",
            "unexpected entry shape")
    require(struct.unpack_from("<I", code, 2)[0] == value("__lab_sp_top") and
            struct.unpack_from("<I", code, 12)[0] == value("__lab_ssp_top"),
            "entry stack immediates differ from symbols")
    require(code[6:10] == b"\x64\xe0\x80\x0e" and code[16:20] == b"\x64\xe0\x80\x0d",
            "entry must set SP then SSP")
    require(code[24:] == b"\x00\x00\xf7\x9e", "entry must park after C returns")

    def instructions(label):
        match = re.search(r"^" + re.escape(label) + r":\s*\n(.*?)(?=\n\w[^\n]*:|\Z)",
                          disassembly, re.M | re.S)
        require(match is not None, "missing disassembly function: " + label)
        return [m.group(2).strip() for m in re.finditer(
            r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2}\s+)+\t(.+)$", match.group(1), re.M)]

    ops = instructions("_start")
    require(len(ops) == 7 and ops[1] == "sp = r0" and ops[3] == "ssp = r0" and
            "<lab_entry_c :" in ops[4] and ops[4].startswith("call ") and
            ops[5] == "nop" and ops[6].startswith("goto -4 "), "entry instruction order")
    calls = [op for op in instructions("lab_entry_c") if op.startswith("call ")]
    require(len(calls) == 2 and "<lab_clear_bytes :" in calls[0] and
            "<lab_synthetic_check :" in calls[1], "clear BSS before synthetic check")
    require(not any(op.startswith("call ") for op in instructions("lab_clear_bytes")),
            "BSS clear depends on another function")
    return dict(status="OFFLINE_LAYOUT_PASSED", entry=entry,
                ram_bytes_used=previous - RAM_BEGIN, stacks_bytes=2 * STACK_BYTES,
                bss_bytes=bss["size"], segments=segments,
                sections=[{k: v for k, v in s.items() if k != "contents"} for s in allocated],
                symbol_values={k: v["value"] for k, v in symbols.items()},
                execution_performed=False, rom_abi_verified=False)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("disassembly", type=Path)
    args = parser.parse_args()
    print(json.dumps(audit(args.elf.read_bytes(), args.disassembly.read_text()), indent=2))
