"""Offline clean-lab: host tests, a WL82 core object or an entry/linker scaffold.

No SDK checkout, stock image, board metadata, downloader or device is used.
This command cannot produce a boot bank or installable firmware image.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from audit_bootlab import audit

ROOT = Path(__file__).resolve().parents[1]
INPUTS = (
    "firmware/bootloader/boot_validation.c",
    "firmware/bootloader/boot_validation.h",
    "firmware/bootlab/CMakeLists.txt",
    "firmware/bootlab/test_validation.c",
    "firmware/bootlab/scenarios.c",
    "firmware/bootlab/startup.c",
    "firmware/bootlab/startup.h",
    "firmware/bootlab/entry.c",
    "firmware/bootlab/entry.S",
    "firmware/bootlab/ram.ld",
    "firmware/bootlab/test_startup.c",
    "scripts/audit_bootlab.py",
    "scripts/test_bootlab_audit.py",
    "scripts/build_bootlab.py",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, capture=False):
    return subprocess.run(
        list(map(str, args)), cwd=ROOT, check=True,
        text=True, encoding="utf-8", errors="replace",
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=("host", "wl82", "wl82-entry"), nargs="?", default="host")
    parser.add_argument("--out", type=Path, default=ROOT / "build/bootlab")
    parser.add_argument("--toolchain", type=Path,
                        default=Path(os.environ["FM1_TOOLCHAIN_DIR"])
                        if "FM1_TOOLCHAIN_DIR" in os.environ else None)
    args = parser.parse_args()
    out = args.out.resolve() / args.target
    out.mkdir(parents=True, exist_ok=True)
    manifest_path = out / "manifest.json"
    manifest = {
        "format": "fm1-clean-lab-evidence/1", "step": 2,
        "target": args.target, "status": "INCOMPLETE", "flashable": False,
        "sdk_checkout_required": False, "vendor_archive_required": False,
        "stock_image_required": False, "device_io_performed": False,
        "source_sha256": {},
        "excluded": ["boot_policy.c", "stock comparison", "board initialization",
                     "ROM symbols", "image packager", "updater/flasher"],
    }
    # Invalidate an earlier success before attempting another build. Failures
    # keep an explicit incomplete report rather than stale successful evidence.
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    try:
        manifest["source_sha256"] = {name: digest(ROOT / name) for name in INPUTS}
        if args.target == "host":
            run(["cmake", "-S", ROOT / "firmware/bootlab", "-B", out])
            run(["cmake", "--build", out, "--config", "Release"])
            run(["ctest", "--test-dir", out, "-C", "Release", "--output-on-failure"])
            run([sys.executable, ROOT / "scripts/test_bootlab_audit.py"])
            executable = out / "Release/bootlab_scenarios.exe"
            if not executable.exists():
                executable = out / ("bootlab_scenarios.exe" if os.name == "nt" else "bootlab_scenarios")
            result = run([executable], capture=True)
            manifest.update(status="HOST_TESTS_PASSED", scenario_output=result.stdout,
                            host_runtime_required=True)
            print(result.stdout, end="")
        else:
            if args.toolchain is None:
                raise ValueError("wl82 requires --toolchain or FM1_TOOLCHAIN_DIR; no SDK archive is needed")
            compiler = args.toolchain.resolve() / "clang.exe"
            nm = args.toolchain.resolve() / "llvm-nm.exe"
            version = run([compiler, "--version"], capture=True)
            obj = out / "boot_validation.o"
            run([compiler, "-target", "pi32v2", "-mcpu=r3", "-integrated-as",
                 "-std=c11", "-Oz", "-ffreestanding", "-fno-common", "-Werror",
                 "-I", ROOT / "firmware/bootloader", "-c",
                 ROOT / "firmware/bootloader/boot_validation.c", "-o", obj])
            symbols = run([nm, "--undefined-only", obj], capture=True)
            if symbols.stdout.strip() or symbols.stderr.strip():
                raise ValueError("WL82 core has unresolved symbols or nm diagnostics: " +
                                 symbols.stdout + symbols.stderr)
            manifest.update(status="WL82_OBJECT_ONLY", undefined_symbols=[],
                            object_sha256=digest(obj), object_bytes=obj.stat().st_size,
                            compiler_version=version.stdout + version.stderr,
                            compiler_sha256=digest(compiler), nm_sha256=digest(nm),
                            target_runtime_required=False)
            print("WL82 validation object compiled; no undefined symbols. Not a bootable loader.")
            if args.target == "wl82-entry":
                linker = args.toolchain.resolve() / "pi32v2-ld.exe"
                objdump = args.toolchain.resolve() / "llvm-objdump.exe"
                objects = []
                flags = ["-target", "pi32v2", "-mcpu=r3", "-integrated-as",
                         "-Oz", "-ffreestanding", "-fno-common", "-Werror"]
                for source in ("entry.S", "entry.c", "startup.c"):
                    output = out / (source.replace(".", "_") + ".o")
                    language = ["-std=c11"] if source.endswith(".c") else []
                    run([compiler, *flags, *language, "-I", ROOT / "firmware/bootloader",
                         "-c", ROOT / "firmware/bootlab" / source, "-o", output])
                    objects.append(output)
                elf, mapfile = out / "bootlab.elf", out / "bootlab.map"
                run([linker, "-nostdlib", "--no-undefined", "--orphan-handling=error",
                     "-T", ROOT / "firmware/bootlab/ram.ld", "-Map", mapfile,
                     "-o", elf, *objects, obj])
                symbols = run([nm, "--undefined-only", elf], capture=True)
                if symbols.stdout.strip() or symbols.stderr.strip():
                    raise ValueError("linked scaffold has unresolved symbols or nm diagnostics")
                disassembly = run([objdump, "-d", elf], capture=True)
                if disassembly.stderr.strip():
                    raise ValueError("objdump diagnostics: " + disassembly.stderr)
                disfile = out / "bootlab.disassembly.txt"
                disfile.write_text(disassembly.stdout, encoding="utf-8")
                inspection = audit(elf.read_bytes(), disassembly.stdout)
                run([sys.executable, ROOT / "scripts/test_bootlab_audit.py",
                     "--elf", elf, "--disassembly", disfile])
                manifest.update(status="WL82_OFFLINE_LINK_PASSED", inspection=inspection,
                                elf_sha256=digest(elf), elf_bytes=elf.stat().st_size,
                                map_sha256=digest(mapfile), disassembly_sha256=digest(disfile),
                                linker_sha256=digest(linker), objdump_sha256=digest(objdump),
                                object_hashes={p.name: digest(p) for p in objects})
                print("Entry, stacks and BSS inspected; target code was NOT executed.")
    except Exception as error:
        manifest.update(status="FAILED", error=str(error))
        raise
    finally:
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Evidence:", manifest_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
