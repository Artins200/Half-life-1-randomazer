#!/usr/bin/env python3
"""Load the built Win32 proxy against a temporary fake original; no game files."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

if len(sys.argv) != 4:
    raise SystemExit("usage: test_bridge_loader_win.py PROXY FAKE LOADER")
proxy, fake, loader = map(lambda p: Path(p).resolve(), sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="chaos_win_bridge_") as directory:
    root = Path(directory)
    present = root / "present" / "hl_randomizer" / "dlls"
    present.mkdir(parents=True)
    shutil.copyfile(proxy, present / "hl.dll")
    shutil.copyfile(fake, present / "hl_orig.dll")
    subprocess.run([str(loader), str(present / "hl.dll"), "present"], cwd=root, check=True, timeout=30)
    of_root = root / "opfor"
    of_mod = of_root / "hl_randomizer_opfor" / "dlls"
    gearbox = of_root / "gearbox" / "dlls"
    valve = of_root / "valve" / "dlls"
    for folder in (of_mod, gearbox, valve):
        folder.mkdir(parents=True)
    shutil.copyfile(proxy, of_mod / "hl.dll")
    shutil.copyfile(fake, of_mod / "hl_orig.dll")
    shutil.copyfile(fake, valve / "hl.dll")
    shutil.copyfile(fake, gearbox / "opfor.dll")
    subprocess.run([str(loader), str(of_mod / "hl.dll"), "opfor"], cwd=root, check=True, timeout=30)
# The production DLL intentionally shows a modal error if the original is
# missing; that interactive path is covered without a dialog by the Linux test.
