#!/usr/bin/env python3
"""Compile/load the production Linux proxy against a fake original game DLL.

This does not replace an in-game test. It exercises actual dlopen, engine hooks,
static entity forwarding, API-version fallback and a missing-original failure.
"""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    if not sys.platform.startswith("linux"):
        print("[SKIP] Linux bridge loader integration test")
        return
    x86 = "--x86" in sys.argv[1:]
    compiler = shlex.split(os.environ.get("ZIG_CXX" if x86 else "CXX", "zig c++" if x86 else "g++"))
    target = ["-target", "x86-linux-gnu.2.7", "-fno-exceptions", "-fno-rtti", "-nostdlib++"] if x86 else []
    with tempfile.TemporaryDirectory(prefix="chaos_bridge_") as temp:
        root = Path(temp)
        mod = root / "with_original" / "hl_randomizer" / "dlls"
        missing = root / "without_original" / "hl_randomizer" / "dlls"
        mod.mkdir(parents=True)
        missing.mkdir(parents=True)
        include = [f"-I{ROOT / 'include'}", f"-I{ROOT / 'src'}"]
        common = ["src/main_dll.cpp", "src/chaos_events.cpp", "src/chaos_catalog.cpp",
                  "src/chaos_features.cpp", "src/death_counter.cpp", "src/dll_bridge_linux.cpp"]
        if x86:
            # Exercise the exact 32-bit .so that will be packaged, not a rebuild.
            shutil.copyfile(ROOT / "hl_randomizer/dlls/hl.so", mod / "hl.so")
        else:
            subprocess.run(compiler + ["-std=c++14", "-shared", "-fPIC", "-fvisibility=hidden"] + include +
                           [str(ROOT / f) for f in common] + ["-ldl", "-lm", "-o", str(mod / "hl.so")], check=True)
        subprocess.run(compiler + target + ["-std=c++14", "-shared", "-fPIC"] + include +
                       [str(ROOT / "tests/fake_original_game.cpp"), "-o", str(mod / "hl_orig.so")], check=True)
        subprocess.run(compiler + target + ["-std=c++14"] + include + [str(ROOT / "tests/test_bridge_loader.cpp"),
                       "-ldl", "-o", str(root / "loader")], check=True)
        shutil.copyfile(mod / "hl.so", missing / "hl.so")
        loader = [str(root / "loader")]
        if x86 and os.environ.get("CHAOS_X86_LD"):
            # Optional private runtime for hosts without system i386 glibc.
            runtime = os.environ.get("CHAOS_X86_LIBDIR")
            if not runtime:
                raise RuntimeError("CHAOS_X86_LIBDIR is required with CHAOS_X86_LD")
            loader = [os.environ["CHAOS_X86_LD"], "--library-path", runtime] + loader
        subprocess.run(loader + [str(mod / "hl.so"), "present"], cwd=root, check=True)
        subprocess.run(loader + [str(missing / "hl.so"), "missing"], cwd=root, check=True)

        # An isolated profile must load the installed Gearbox DLL and forward
        # OF weapons/projectiles, even with a conflicting local/Valve DLL.
        of_root = root / "with_opfor"
        of_mod = of_root / "hl_randomizer_opfor" / "dlls"
        gearbox = of_root / "gearbox" / "dlls"
        valve = of_root / "valve" / "dlls"
        for folder in (of_mod, gearbox, valve):
            folder.mkdir(parents=True)
        shutil.copyfile(mod / "hl.so", of_mod / "hl.so")
        shutil.copyfile(mod / "hl_orig.so", of_mod / "hl_orig.so")
        shutil.copyfile(mod / "hl_orig.so", valve / "hl.so")
        shutil.copyfile(mod / "hl_orig.so", gearbox / "opfor.so")
        subprocess.run(loader + [str(of_mod / "hl.so"), "opfor"], cwd=root, check=True)
        (gearbox / "opfor.so").unlink()
        shutil.copyfile(mod / "hl_orig.so", gearbox / "opfor_i386.so")
        subprocess.run(loader + [str(of_mod / "hl.so"), "opfor_i386"], cwd=root, check=True)
        (gearbox / "opfor_i386.so").unlink()
        subprocess.run(loader + [str(of_mod / "hl.so"), "opfor_missing"], cwd=root, check=True)


if __name__ == "__main__":
    main()
