"""Offline WL82 user-boot build attempt and validated binary comparison.

Never invokes vendor post-build/download tools or accesses a device. External
vendor sources, archives and decoded FM1 code remain outside this repository.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

BOOT_PIN = 'ff3b9299d1b94d834b995fd858b3f75e2faa9840'
SDK_PIN = 'e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d'
STOCK_SHA = 'fcaf033c5e10526353ecb11279a7556ac8645026d370c0267912cb4bfa7c7e4d'
SDK_CODE_SHA = '0981b66bbf32a6be9b0e371a9a7607efb6c9260c604271e5c86046f376403750'
BASE = 0x1c02000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def crc16(data):
    value = 0
    for byte in data:
        value ^= byte << 8
        for _ in range(8):
            value = ((value << 1) ^ (0x1021 if value & 0x8000 else 0)) & 0xffff
    return value


def decode_sdk_bank(data):
    if len(data) < 16:
        raise ValueError('Truncated SDK bank')
    count, size, load, offset, code_crc, header_crc = struct.unpack_from('<HHIIHH', data)
    if count != 1 or load != BASE or offset != 16 or len(data) != offset + size:
        raise ValueError('Unreviewed SDK bank layout')
    if crc16(data[:14]) != header_crc or crc16(data[offset:]) != code_crc:
        raise ValueError('SDK bank CRC mismatch')
    code = data[offset:]
    # Reviewed uncompressed WL82 startup begins with a push and literal load.
    # Do not treat compressed bytes as instructions.
    if code[:6] != bytes.fromhex('d8 e8 07 00 c0 ff'):
        raise ValueError('Unreviewed/compressed SDK startup')
    return code, {'code_bytes': size, 'load_address': hex(load),
                  'bank_bytes': len(data), 'header_and_data_crc_valid': True,
                  'code_sha256': sha(code), 'bank_sha256': sha(data)}


def inspect_boot(code, sdk=False):
    """Check reviewed instruction bytes before interpreting fixed addresses."""
    expected_sha = SDK_CODE_SHA if sdk else STOCK_SHA
    if sha(code) != expected_sha:
        raise ValueError('Unreviewed boot code')
    evidence = {
        0x1c02010: 'c0 ff b0 58 c0 01' if sdk else 'c0 ff 40 58 c0 01',
        0x1c02018: 'c2 ff 70 0a 00 00' if sdk else 'c2 ff 60 0d 00 00',
        0x1c02036: 'ed ff b0 5c c0 01' if sdk else 'ed ff 40 5c c0 01',
        0x1c0203c: 'ee ff b0 60 c0 01' if sdk else 'ee ff 40 60 c0 01',
    }
    evidence.update({
        0x1c04e0e: 'cb ff 80 fd c7 01',
        0x1c05008: 'd0 ec c4 01 00 55',
        0x1c0500e: '42 20 43 20 50 ec b9 2d',
        0x1c05236: '00 e1 88 b0',
        0x1c0523e: '01 16 10 85 52 05 92 05',
        0x1c05246: '09 98 81 60',
        0x1c0524e: '10 9f 02 07 92 07',
        0x1c052ea: 'd0 ec c0 10 00 e1 88 b0 c1 00',
    } if sdk else {
        0x1c04dc0: 'ca ff 80 fd c7 01',
        0x1c04f9e: 'd0 ec c4 01 00 55',
        0x1c04fa4: '42 20 43 20 50 ec a9 2d',
        0x1c051d0: '00 e1 88 a0',
        0x1c051d8: '01 16 10 85 52 05 92 05',
        0x1c051e0: '09 98 81 60',
        0x1c051e8: '10 9f 02 07 92 07',
        0x1c05284: 'd0 ec c0 10 00 e1 88 a0 c1 00',
    })
    for address, expected in evidence.items():
        expected = bytes.fromhex(expected)
        if code[address-BASE:address-BASE+len(expected)] != expected:
            raise ValueError(f'Boot instruction mismatch at {address:#x}')
    return {
        'bss_start': hex(struct.unpack_from('<I', code, 0x12)[0]),
        'bss_bytes': struct.unpack_from('<I', code, 0x1a)[0],
        'initial_ssp': hex(struct.unpack_from('<I', code, 0x38)[0]),
        'initial_sp': hex(struct.unpack_from('<I', code, 0x3e)[0]),
        'argument_address': '0x1c7fe08', 'prefix_bytes_copied': 24,
        'flash_header_address': '0x1c7fe20', 'flash_header_bytes_copied': 32,
        'application_call': hex(0x1c052f2 if sdk else 0x1c0528c),
        'plus80_initialization': 'conditional on metadata pointer',
        'plus88_initialization': 'not established; not proof of absence',
        'usb_update_marker_present': b'usb_update_mode\0' in code,
        'checked_instruction_sites': len(evidence),
    }


def analysis_elf(data):
    """Synthetic ELF for disassembly only; not a linked/flashable loader."""
    strings = b'\0.sweep\0.shstrtab\0'
    offset = 0x100
    names = offset + len(data)
    shoff = (names + len(strings) + 3) & ~3
    ident = b'\x7fELF\x01\x01\x01' + bytes(9)
    header = struct.pack('<16sHHIIIIIHHHHHH', ident, 2, 0xf1, 1, BASE,
                         52, shoff, 0, 52, 32, 1, 40, 3, 2)
    program = struct.pack('<IIIIIIII', 1, offset, BASE, BASE,
                          len(data), len(data), 5, 1)
    body = header + program
    body += bytes(offset - len(body)) + data + strings
    body += bytes(shoff - len(body)) + bytes(40)
    body += struct.pack('<IIIIIIIIII', 1, 1, 6, BASE, offset, len(data), 0, 0, 2, 0)
    body += struct.pack('<IIIIIIIIII', 8, 3, 0, 0, names, len(strings), 0, 0, 1, 0)
    return body


def make_list(text, name):
    match = re.search(r'^' + re.escape(name) + r'\s*:=\s*\\\n((?:.*\\\n)*)', text, re.M)
    if not match:
        raise ValueError('Missing upstream list: ' + name)
    return match.group(1).replace('\\\n', ' ').split()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--boot-sdk', required=True, type=Path)
    parser.add_argument('--app-sdk', required=True, type=Path)
    parser.add_argument('--stock-code', required=True, type=Path,
                        help='Already decoded, hash-pinned normal boot code only')
    parser.add_argument('--toolchain', type=Path, default=Path('C:/JL/pi32/bin'))
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    boot, sdk, tc, out = [p.resolve() for p in
                          (args.boot_sdk, args.app_sdk, args.toolchain, args.out)]
    out.mkdir(parents=True, exist_ok=True)
    report = {'status': 'BUILDING_OR_FAILED', 'flashable': False,
              'device_operations_performed': False, 'post_build_executed': False}
    manifest = out / 'study.json'
    manifest.write_text(json.dumps(report, indent=2) + '\n')
    with (out / 'build.log').open('w', encoding='utf8') as log:
        def run(command, cwd=out):
            command = list(map(str, command))
            result = subprocess.run(command, cwd=cwd, capture_output=True,
                                    text=True, errors='replace', timeout=60)
            log.write(json.dumps(command) + '\n' + result.stdout + result.stderr)
            log.flush()
            return result

        for path, pin in ((boot, BOOT_PIN), (sdk, SDK_PIN)):
            result = run(['git', '-C', path, 'rev-parse', 'HEAD'])
            if result.returncode or result.stdout.strip() != pin:
                raise ValueError('Wrong SDK revision: ' + str(path))
            result = run(['git', '-C', path, 'status', '--porcelain', '--untracked-files=no'])
            if result.returncode or result.stdout.strip():
                raise ValueError('Modified SDK: ' + str(path))
        project = boot / 'user_boot/cpu/wl82'
        make = (project / 'Makefile').read_text(encoding='utf8')
        flags = make_list(make, 'CFLAGS')
        defines = make_list(make, 'DEFINES')
        sys_inc = tc.parent / 'pi32v2-include'
        includes = ['-I' + str(sys_inc) if s == '-I$(SYS_INC_DIR)' else
                    '-I' + str((project / s[2:]).resolve())
                    for s in make_list(make, 'INCLUDES')]
        sources = [(project / s).resolve() for s in make_list(make, 'c_SRC_FILES')]
        dependencies = [project / 'Makefile', project / 'output/ram_ld.c',
                        project / 'output/maskrom_stubs.ld', *sources,
                        *sorted((boot / 'user_boot/include_lib').rglob('*.h')),
                        *sorted((boot / 'user_boot/app/inc').rglob('*.h'))]
        report.update(boot_sdk_commit=BOOT_PIN, app_sdk_commit=SDK_PIN,
                      study_script_sha256=sha(Path(__file__).read_bytes()),
                      source_sha256={str(p.relative_to(boot)): sha(p.read_bytes())
                                     for p in dependencies},
                      tool_sha256={p.name: sha(p.read_bytes()) for p in
                                   (tc / 'clang.exe', tc / 'pi32v2-lto-wrapper.exe',
                                    tc / 'llvm-objdump.exe')})
        library = boot / 'user_boot/include_lib/liba/wl82/uboot.a'
        stubs = project / 'output/maskrom_stubs.ld'
        report['missing_inputs'] = []
        if not library.is_file():
            report['missing_inputs'].append('user_boot/include_lib/liba/wl82/uboot.a')
        report['rom_exports_review'] = ('WL82 maskrom_stubs.ld is empty; required exports '
                                        'cannot be established without the matching archive'
                                        if not stubs.read_bytes().strip() else
                                        'Definitions present; linked review still required')
        result = run([tc / 'clang.exe', *flags, *defines, *includes,
                      '-D__LD__', '-E', '-P', project / 'output/ram_ld.c', '-o', out / 'ram.ld'])
        if result.returncode:
            raise RuntimeError('Linker-script preprocessing failed; see build.log')
        report['compiled'] = []
        objects = []
        for i, source in enumerate(sources):
            obj = out / f'{i}-{source.stem}.o'
            # Keep compiler debug-directory metadata stable across output dirs.
            result = run([tc / 'clang.exe', *flags, *defines, *includes,
                          '-c', source, '-o', obj], cwd=project)
            report['compiled'].append({'source': str(source.relative_to(boot)),
                                       'returncode': result.returncode,
                                       'object_sha256': sha(obj.read_bytes())
                                       if result.returncode == 0 else None})
            if result.returncode == 0:
                objects.append(obj)
        if len(objects) == len(sources):
            # Retain upstream link options, rewrite only output/input paths.
            lflags = []
            for flag in make_list(make, 'LFLAGS'):
                if flag.startswith('-M='):
                    flag = '-M=' + str(out / 'map.txt')
                elif flag.startswith('-T'):
                    flag = '-T' + str(out / 'ram.ld')
                elif flag.startswith('../'):
                    flag = str((project / flag).resolve())
                lflags.append(flag)
            sys_lib = tc.parent / 'pi32v2-lib/r3'
            libraries = [sys_lib / name for name in ('libm.a', 'libc.a', 'libcompiler-rt.a')]
            report['system_library_sha256'] = {p.name: sha(p.read_bytes()) for p in libraries}
            result = run([tc / 'pi32v2-lto-wrapper.exe', '-o', out / 'uboot.elf',
                          *objects, *lflags, '-L' + str(sys_lib), *libraries])
            report['link_returncode'] = result.returncode
            report['link_diagnostic'] = (result.stdout + result.stderr)[-4000:]
        else:
            report['link_returncode'] = None
        report['status'] = ('LINKED_UNQUALIFIED' if report['link_returncode'] == 0 else
                            'SOURCE_COMPILED_LINK_BLOCKED' if len(objects) == len(sources) else
                            'SOURCE_COMPILE_FAILED')

        stock = args.stock_code.read_bytes()
        if sha(stock) != STOCK_SHA:
            raise ValueError('Unreviewed FM1 boot code; refuse comparison')
        bank_path = sdk / 'cpu/wl82/tools/uboot.boot'
        code, bank = decode_sdk_bank(bank_path.read_bytes())
        bank['reviewed_startup_and_handoff'] = inspect_boot(code, sdk=True)
        report['comparison'] = {
            'reference_kind': 'SDK-shipped prebuilt, NOT rebuilt user-boot source',
            'sdk_prebuilt': bank,
            'fm1_stock': {'code_bytes': len(stock), 'load_address': hex(BASE),
                          'code_sha256': STOCK_SHA,
                          'reviewed_startup_and_handoff': inspect_boot(stock)},
            'byte_identical': code == stock,
            'same_offset_equal_bytes': sum(a == b for a, b in zip(code, stock)),
            'size_delta_bytes': len(code) - len(stock),
            'hardware_verified': False,
        }
        for name, data in (('sdk-prebuilt', code), ('fm1-stock', stock)):
            elf = out / f'{name}-analysis-only.elf'
            elf.write_bytes(analysis_elf(data))
            result = run([tc / 'llvm-objdump.exe', '-disassemble-all',
                          '-arch-name=pi32v2', '-mcpu=r3', '-print-imm-hex', elf])
            if result.returncode:
                raise RuntimeError('Disassembly failed')
            (out / f'{name}-linear-sweep.asm').write_text(result.stdout, encoding='utf8')
            (out / f'{name}-code.bin').write_bytes(data)
        manifest.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print(json.dumps({k: report[k] for k in
                          ('status', 'missing_inputs', 'rom_exports_review', 'link_returncode', 'comparison',
                           'flashable', 'device_operations_performed')}, indent=2))


if __name__ == '__main__':
    main()
