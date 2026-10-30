#!/usr/bin/env python3
"""
Binary verification for Half-Life 1 Randomizer Chaos Mod:
1. Validates hl_randomizer/dlls/hl.dll (Win32 PE32 i386):
   - Machine == IMAGE_FILE_MACHINE_I386 (0x014c), Magic == PE32 (0x010b)
   - Exports the five unmangled GoldSrc entry points plus HL1 and OF entity wrappers
   - Every export points to executable code within this PE image
   - Export names are strictly sorted lexicographically (required for Win32 GetProcAddress binary search)
   - GiveFnptrsToDll uses __stdcall (contains 'ret 8' -> 0xC2 0x08 0x00)
   - Zero non-system MinGW DLL dependencies (no libgcc, libstdc++, libwinpthread)
2. Validates hl_randomizer/dlls/hl.so (Linux ELF32 i386):
   - ELF32 little-endian i386 shared object
   - Exports GiveFnptrsToDll, GetEntityAPI, GetEntityAPI2 + all tracked HL1/OF symbols
3. Checks that the independent OF profile contains byte-identical proxy binaries.
"""

import os
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HL_ENTITY_NAMES = re.findall(r"HL_ENTITY_LINK\((\w+)\)",
                             (ROOT / "src/hl_entity_list.inc").read_text(encoding="utf-8"))
OPFOR_ENTITY_NAMES = re.findall(r"HL_ENTITY_LINK\((\w+)\)",
                                (ROOT / "src/opfor_entity_list.inc").read_text(encoding="utf-8"))
ENTITY_NAMES = HL_ENTITY_NAMES + OPFOR_ENTITY_NAMES
assert len(HL_ENTITY_NAMES) == 250 and len(OPFOR_ENTITY_NAMES) == 107
assert len(set(ENTITY_NAMES)) == len(ENTITY_NAMES)
for name in ("weapon_eagle", "weapon_m249", "weapon_knife", "weapon_displacer", "spore", "grapple_tip"):
    assert name in OPFOR_ENTITY_NAMES


def verify_win32_pe(dll_path: str) -> None:
    assert os.path.isfile(dll_path), f"Missing Win32 DLL: {dll_path}"
    data = open(dll_path, "rb").read()
    assert data[:2] == b"MZ", "Invalid DOS MZ header"

    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    sig, machine, num_sec = struct.unpack_from("<IHH", data, e_lfanew)
    assert sig == 0x4550, "Invalid PE signature"
    assert machine == 0x014C, f"Expected i386 (0x014c), got 0x{machine:04x}"

    opt_size = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    opt_off = e_lfanew + 24
    magic = struct.unpack_from("<H", data, opt_off)[0]
    assert magic == 0x010B, f"Expected 32-bit PE32 (0x010b), got 0x{magic:04x}"

    exp_rva, _ = struct.unpack_from("<II", data, opt_off + 96)
    imp_rva, _ = struct.unpack_from("<II", data, opt_off + 104)
    assert exp_rva != 0, "Missing PE export directory"

    secs = []
    sec_off = opt_off + opt_size
    for i in range(num_sec):
        name, vsize, vrva, rsize, roff = struct.unpack_from("<8sIIII", data, sec_off + i * 40)
        flags = struct.unpack_from("<I", data, sec_off + i * 40 + 36)[0]
        secs.append((name.rstrip(b"\x00").decode(errors="replace"), vrva, vsize, roff, rsize, flags))

    def rva2off(rva: int) -> int:
        for _, vrva, vsize, roff, rsize, _ in secs:
            if vrva <= rva < vrva + max(vsize, rsize):
                return roff + (rva - vrva)
        raise AssertionError(f"Unmapped RVA 0x{rva:x}")

    def cstr(off: int) -> str:
        end = data.index(b"\x00", off)
        return data[off:end].decode("ascii")

    exp_off = rva2off(exp_rva)
    _, _, _, _, _, base, nfuncs, nnames, afuncs, anames, aords = struct.unpack_from(
        "<IIHHIIIIIII", data, exp_off
    )

    exports = {}
    export_name_list = []
    for i in range(nnames):
        name_rva = struct.unpack_from("<I", data, rva2off(anames) + i * 4)[0]
        ord_idx = struct.unpack_from("<H", data, rva2off(aords) + i * 2)[0]
        fn_rva = struct.unpack_from("<I", data, rva2off(afuncs) + ord_idx * 4)[0]
        name = cstr(rva2off(name_rva))
        export_name_list.append(name)
        exports[name] = (base + ord_idx, fn_rva)

    assert export_name_list == sorted(export_name_list), (
        f"PE Export names not lexicographically sorted: {export_name_list}"
    )

    required = [
        "GiveFnptrsToDll",
        "GetEntityAPI",
        "GetEntityAPI2",
        "GetNewDLLFunctions",
        "Server_GetBlendingInterface",
    ]
    for req in required + ENTITY_NAMES:
        assert req in exports, f"Missing required GoldSrc export '{req}' in {dll_path}"
    for name in required + ENTITY_NAMES:
        _, fn_rva = exports[name]
        assert any(vrva <= fn_rva < vrva + max(vsize, rsize) and (flags & 0x20000000)
                   for _, vrva, vsize, _, rsize, flags in secs), (
            f"{name} must be a real executable export inside the PE, not a fabricated RVA")

    give_ord, give_rva = exports["GiveFnptrsToDll"]
    assert give_ord == 1, f"GiveFnptrsToDll should have ordinal 1, got {give_ord}"
    give_code = data[rva2off(give_rva) : rva2off(give_rva) + 256]
    assert b"\xc2\x08\x00" in give_code, (
        "GiveFnptrsToDll must use Win32 __stdcall ('ret 8' = C2 08 00)!"
    )

    # Check imported DLLs: forbid external MinGW runtime DLLs
    forbidden_substrings = ("libgcc", "libstdc++", "libwinpthread", "vcruntime140d", "msvcp140d", "api-ms-win-crt-private")
    imported_dlls = []
    if imp_rva:
        imp_off = rva2off(imp_rva)
        idx = 0
        while True:
            _, _, _, name_rva, iat = struct.unpack_from("<IIIII", data, imp_off + idx * 20)
            if not name_rva and not iat:
                break
            dll_name = cstr(rva2off(name_rva))
            imported_dlls.append(dll_name)
            low = dll_name.lower()
            for bad in forbidden_substrings:
                assert bad not in low, f"Forbidden runtime dependency '{dll_name}' in {dll_path}"
            idx += 1

    print(f"[OK] {dll_path}: Valid 32-bit i386 PE32, {len(exports)} exports verified, imports={imported_dlls}")


def verify_linux_elf(so_path: str) -> None:
    if not os.path.isfile(so_path):
        print(f"[SKIP] {so_path} not present in this build environment")
        return

    data = open(so_path, "rb").read()
    assert data[:4] == b"\x7fELF", "Invalid ELF magic"
    ei_class, ei_data = data[4], data[5]
    assert ei_class == 1, f"Expected 32-bit ELFCLASS32 (1), got {ei_class}"
    assert ei_data == 1, f"Expected little-endian ELF (1), got {ei_data}"

    e_type, e_machine = struct.unpack_from("<HH", data, 16)
    assert e_type == 3, f"Expected ET_DYN shared object (3), got {e_type}"
    assert e_machine == 3, f"Expected EM_386 i386 (3), got {e_machine}"

    # Inspect actual defined dynamic symbols, not arbitrary strings in the file.
    shoff = struct.unpack_from("<I", data, 32)[0]
    shentsize, shnum = struct.unpack_from("<HH", data, 46)
    assert shoff and shentsize >= 40 and shnum, "Missing ELF section headers"
    sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    exports = set()
    for sec in sections:
        if sec[1] != 11:  # SHT_DYNSYM
            continue
        _, _, _, _, symoff, symsize, link, _, _, entsize = sec
        assert 0 <= link < len(sections) and entsize >= 16
        strsec = sections[link]
        strings = data[strsec[4] : strsec[4] + strsec[5]]
        for off in range(symoff, symoff + symsize, entsize):
            nameoff, value, _, info, visibility, shndx = struct.unpack_from("<IIIBBH", data, off)
            if not nameoff or not value or not shndx or info >> 4 != 1 or (visibility & 3) != 0:
                continue
            end = strings.index(b"\x00", nameoff)
            exports.add(strings[nameoff:end].decode("ascii"))
    for sym in ["GiveFnptrsToDll", "GetEntityAPI", "GetEntityAPI2"] + ENTITY_NAMES:
        assert sym in exports, f"Missing defined exported function '{sym}' in {so_path}"

    print(f"[OK] {so_path}: Valid 32-bit i386 ELF shared library ({len(data)} bytes)")


def verify_hud_installers() -> None:
    exe = ROOT / "hl_randomizer_opfor/install_hev_hud.exe"
    data = exe.read_bytes()
    assert data[:2] == b"MZ"
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    sig, machine, count = struct.unpack_from("<IHH", data, pe)
    assert sig == 0x4550 and machine == 0x14c
    opt = pe + 24
    assert struct.unpack_from("<H", data, opt)[0] == 0x10b
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    sec = opt + optsize
    sections = [struct.unpack_from("<8sIIII", data, sec + 40 * i) for i in range(count)]

    def offset(rva):
        for _, size, va, rawsize, raw in sections:
            if va <= rva < va + max(size, rawsize):
                return raw + rva - va
        raise AssertionError(f"Unmapped installer RVA {rva:x}")

    imports = []
    imp = struct.unpack_from("<I", data, opt + 104)[0]
    assert imp
    idx = offset(imp)
    while True:
        _, _, _, name_rva, iat = struct.unpack_from("<IIIII", data, idx)
        if not name_rva and not iat:
            break
        pos = offset(name_rva)
        name = data[pos:data.index(b"\x00", pos)].decode("ascii").lower()
        assert not any(x in name for x in ("api-ms-win-crt-private", "libgcc", "libstdc++", "libwinpthread")), name
        imports.append(name)
        idx += 20
    linux = (ROOT / "hl_randomizer_opfor/install_hev_hud_linux").read_bytes()
    assert linux[:6] == b"\x7fELF\x02\x01" and struct.unpack_from("<H", linux, 18)[0] == 62
    print(f"[OK] HEV HUD installers: Win32 PE with public runtime imports, Linux x86-64 ELF ({len(imports)} imports)")


if __name__ == "__main__":
    verify_win32_pe("hl_randomizer/dlls/hl.dll")
    verify_linux_elf("hl_randomizer/dlls/hl.so")
    verify_hud_installers()
    for name in ("hl.dll", "hl.so"):
        stock = ROOT / "hl_randomizer/dlls" / name
        opfor = ROOT / "hl_randomizer_opfor/dlls" / name
        assert opfor.is_file() and stock.read_bytes() == opfor.read_bytes(), (
            f"The OF profile must contain the same packaged proxy: {opfor}")
    print("ALL BINARY ABI, EXPORT AND PROFILE COPY CHECKS PASSED!")
