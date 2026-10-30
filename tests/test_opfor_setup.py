#!/usr/bin/env python3
"""Test the OF client setup with tiny fake files, never with game data."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
WINDOWS = sys.platform == "win32"
if not (WINDOWS or sys.platform.startswith("linux")):
    print("[SKIP] OF client setup is supported on Windows/Linux")
    raise SystemExit(0)

script_name = "install_client.bat" if WINDOWS else "install_client.sh"
client_name = "client.dll" if WINDOWS else "client.so"
original_name = "opfor.dll" if WINDOWS else "opfor.so"
proxy_name = "hl.dll" if WINDOWS else "hl.so"
environment = os.environ.copy()
environment["CHAOS_INSTALL_NO_PAUSE"] = "1"

with tempfile.TemporaryDirectory(prefix="chaos setup ") as tmp:
    root = Path(tmp) / "Half-Life with spaces"
    mod = root / "hl_randomizer_opfor"
    gearbox = root / "gearbox"
    (mod / "dlls").mkdir(parents=True)
    shutil.copyfile(ROOT / "hl_randomizer_opfor" / script_name, mod / script_name)
    (mod / "dlls" / proxy_name).write_bytes(b"fake proxy")
    (mod / "SAVE").mkdir()
    save = mod / "SAVE" / "important.sav"
    save.write_bytes(b"do not change this save")

    def run():
        command = ["cmd", "/d", "/c", script_name] if WINDOWS else ["sh", str(mod / script_name)]
        return subprocess.run(command, cwd=mod, env=environment, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=20)

    # Missing installed expansion: fail without creating/copying a client.
    assert run().returncode != 0
    target = mod / "cl_dlls" / client_name
    assert not target.exists() and save.read_bytes() == b"do not change this save"

    (gearbox / "dlls").mkdir(parents=True)
    (gearbox / "cl_dlls").mkdir(parents=True)
    (gearbox / "dlls" / original_name).write_bytes(b"fake OF server")
    source = gearbox / "cl_dlls" / client_name
    source.write_bytes(b"fake OF client")
    assert run().returncode == 0
    assert target.read_bytes() == source.read_bytes() == b"fake OF client"
    assert run().returncode == 0  # idempotent if identical

    # A pre-existing different client is neither replaced nor removed.
    target.write_bytes(b"custom client")
    assert run().returncode != 0
    assert target.read_bytes() == b"custom client"
    assert source.read_bytes() == b"fake OF client"
    assert save.read_bytes() == b"do not change this save"
    if not WINDOWS:
        target.unlink()
        target.symlink_to(source)
        assert run().returncode != 0
        assert target.is_symlink() and source.read_bytes() == b"fake OF client"

print("[OK] OF client setup: missing install, copy, idempotence, overwrite protection, untouched save")
