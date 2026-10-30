#!/usr/bin/env python3
"""Test local HEV HUD overlay creation with synthetic files only."""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
if len(sys.argv) != 2:
    raise SystemExit("usage: test_hev_hud_setup.py <built installer>")
PROGRAM = Path(sys.argv[1]).resolve()
WINDOWS = sys.platform == "win32"
if not (WINDOWS or sys.platform.startswith("linux")):
    print("[SKIP] local HEV HUD installer is supported on Windows/Linux")
    raise SystemExit(0)

ROLES = [*(f"number_{i}" for i in range(10)), "cross", "suit_full", "suit_empty"]
RESOLUTIONS = (320, 640, 1280, 2560)
SPRITES = b"IDSP" + struct.pack("<iiifiii fi", 2, 0, 2, 1.0, 256, 256, 1, 0.0, 0)
assert len(SPRITES) == 40
SPRITES += b"synthetic sprite payload"


def make_list(which):
    entries = []
    for res in RESOLUTIONS:
        source = f"{res}/sheet" if which == "hl" else f"{res}ofsheet"
        # Keep OF-only entries in their original sequence and on OF sheets.
        entries.append(f"selection {res} {res}ofsheet 2 2 10 10")
        for i, role in enumerate(ROLES):
            entries.append(f"{role} {res} {source} {i * 2} 3 2 4")
        entries.append(f"of_extra {res} {res}ofsheet 4 4 8 8")
    return str(len(entries)) + "\n// Synthetic lists; not game assets\n" + "\n".join(entries) + "\n"


def create_install(root):
    mod, valve, gearbox = root / "hl_randomizer_opfor", root / "valve", root / "gearbox"
    for folder in (mod, valve / "sprites", gearbox / "sprites"):
        folder.mkdir(parents=True)
    executable = mod / ("install_hev_hud.exe" if WINDOWS else "install_hev_hud_linux")
    shutil.copy2(PROGRAM, executable)
    if not WINDOWS:
        executable.chmod(0o755)
    (mod / "SAVE").mkdir()
    (mod / "SAVE" / "old.sav").write_bytes(b"save sentinel")
    hl_txt, of_txt = make_list("hl"), make_list("of")
    (valve / "sprites" / "hud.txt").write_text(hl_txt, encoding="ascii")
    (gearbox / "sprites" / "hud.txt").write_text(of_txt, encoding="ascii")
    for res in RESOLUTIONS:
        folder = valve / "sprites" / str(res)
        folder.mkdir()
        (folder / "sheet.spr").write_bytes(SPRITES + str(res).encode())
    return mod, valve, gearbox, executable, hl_txt, of_txt


def run(executable):
    return subprocess.run([str(executable)], cwd=executable.parent.parent, capture_output=True, timeout=20)


def entries(text):
    return [line.split() for line in text.splitlines()[2:] if line.strip() and not line.startswith("//")]


with tempfile.TemporaryDirectory(prefix="chaos hud test ") as tmp:
    root = Path(tmp) / "Half Life unicode path \u0418\u0433\u0440\u044b"
    mod, valve, gearbox, executable, hl_txt, of_txt = create_install(root)
    result = run(executable)
    assert result.returncode == 0, result.stdout + result.stderr
    generated = (mod / "sprites" / "hud.txt").read_text(encoding="ascii")
    before, after = entries(of_txt), entries(generated)
    assert len(before) == len(after) == int(generated.splitlines()[0])
    for old, new in zip(before, after):
        assert old[:2] == new[:2]
        if old[0] not in ROLES:
            assert new == old, (old, new)  # OF-only entries remain intact and in order
        else:
            assert new[2].startswith("chhev_") and "/" not in new[2]
            assert new[3:] == old[3:]
            assert (mod / "sprites" / (new[2] + ".spr")).read_bytes() == SPRITES + new[1].encode()
    assert (mod / "SAVE" / "old.sav").read_bytes() == b"save sentinel"
    assert (valve / "sprites" / "hud.txt").read_text(encoding="ascii") == hl_txt
    assert (gearbox / "sprites" / "hud.txt").read_text(encoding="ascii") == of_txt
    assert run(executable).returncode == 0  # idempotent

    # Never overwrite a custom HUD, nor even copy assets after detecting one.
    (mod / "sprites" / "hud.txt").write_bytes(b"custom user HUD")
    before_paths = {p.name: p.read_bytes() for p in (mod / "sprites").iterdir()}
    assert run(executable).returncode != 0
    assert {p.name: p.read_bytes() for p in (mod / "sprites").iterdir()} == before_paths
    assert (mod / "SAVE" / "old.sav").read_bytes() == b"save sentinel"

with tempfile.TemporaryDirectory(prefix="chaos hud reject ") as tmp:
    mod, valve, gearbox, executable, hl_txt, of_txt = create_install(Path(tmp))
    # Missing or invalid original sprites fail before any output is created.
    missing = valve / "sprites" / "1280" / "sheet.spr"
    missing.unlink()
    assert run(executable).returncode != 0 and not (mod / "sprites").exists()
    missing.write_bytes(b"bad sprite")
    assert run(executable).returncode != 0 and not (mod / "sprites").exists()
    missing.write_bytes(SPRITES + b"1280")
    # An out-of-bounds rectangle and a traversal path are both rejected.
    hl_file = valve / "sprites" / "hud.txt"
    hl_file.write_text(hl_txt.replace("cross 640 640/sheet 20 3 2 4", "cross 640 640/sheet 255 3 2 4"), encoding="ascii")
    assert run(executable).returncode != 0 and not (mod / "sprites").exists()
    hl_file.write_text(hl_txt.replace("320/sheet", "../unsafe"), encoding="ascii")
    assert run(executable).returncode != 0 and not (mod / "sprites").exists()
    hl_file.write_text(hl_txt, encoding="ascii")
    # Broken number ordering would make the client draw the wrong digits.
    bad_of = of_txt.replace("number_2 640", "number_9 640", 1)
    (gearbox / "sprites" / "hud.txt").write_text(bad_of, encoding="ascii")
    assert run(executable).returncode != 0 and not (mod / "sprites").exists()
    (gearbox / "sprites" / "hud.txt").write_text(of_txt, encoding="ascii")
    if not WINDOWS:
        (mod / "sprites").symlink_to(valve / "sprites", target_is_directory=True)
        assert run(executable).returncode != 0 and (mod / "sprites").is_symlink()
        assert (valve / "sprites" / "hud.txt").read_text(encoding="ascii") == hl_txt
    assert (mod / "SAVE" / "old.sav").read_bytes() == b"save sentinel"

# The ordinary client installer invokes the HUD helper on both platforms,
# including when the Gearbox client has already been copied in a prior run.
with tempfile.TemporaryDirectory(prefix="chaos hud and client ") as tmp:
    mod, valve, gearbox, executable, _, _ = create_install(Path(tmp))
    (mod / "dlls").mkdir()
    (gearbox / "dlls").mkdir()
    (gearbox / "cl_dlls").mkdir()
    if WINDOWS:
        script = "install_client.bat"
        (mod / "dlls" / "hl.dll").write_bytes(b"proxy")
        (gearbox / "dlls" / "opfor.dll").write_bytes(b"server")
        (gearbox / "cl_dlls" / "client.dll").write_bytes(b"client")
        target = mod / "cl_dlls" / "client.dll"
        command = ["cmd", "/d", "/c", script]
    else:
        script = "install_client.sh"
        (mod / "dlls" / "hl.so").write_bytes(b"proxy")
        (gearbox / "dlls" / "opfor.so").write_bytes(b"server")
        (gearbox / "cl_dlls" / "client.so").write_bytes(b"client")
        target = mod / "cl_dlls" / "client.so"
        command = ["sh", str(mod / script)]
    shutil.copy2(ROOT / "hl_randomizer_opfor" / script, mod / script)
    env = dict(os.environ, CHAOS_INSTALL_NO_PAUSE="1")
    for _ in range(2):
        result = subprocess.run(command, cwd=mod, env=env, capture_output=True, timeout=20)
        assert result.returncode == 0, result.stdout + result.stderr
        assert target.read_bytes() == b"client" and (mod / "sprites" / "hud.txt").is_file()
        assert (mod / "SAVE" / "old.sav").read_bytes() == b"save sentinel"

print("[OK] HEV HUD installer: four resolutions, OF entries preserved, safety, idempotence and client setup")
