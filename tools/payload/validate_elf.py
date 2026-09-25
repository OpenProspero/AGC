#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
"""Fail-closed validator for a console payload ELF.

An artifact must pass every check here before it may be pushed. This is a
hard rule: an earlier session pushed a deliberately malformed ELF as a
diagnostic and crashed the operator's loader, which cost a console
restart. Nothing unvalidated may be sent, ever.

usage: validate_elf.py <payload.elf>   (exit 0 = safe to push)
"""

import struct
import sys

ELF_HEADER = 64
PROGRAM_HEADER = 56
SECTION_HEADER = 64

ET_DYN = 3
ET_EXEC = 2
EM_X86_64 = 62
PT_LOAD = 1
SHT_RELA = 4

MAX_PAYLOAD_BYTES = 32 * 1024 * 1024


def validate(path: str) -> tuple[bool, str]:
    try:
        with open(path, "rb") as handle:
            blob = handle.read()
    except OSError as exc:
        return False, f"cannot read: {exc}"

    if len(blob) < ELF_HEADER:
        return False, f"too small for an ELF header ({len(blob)} bytes)"
    if len(blob) > MAX_PAYLOAD_BYTES:
        return False, f"larger than the {MAX_PAYLOAD_BYTES} byte limit"

    if blob[0:4] != b"\x7fELF":
        return False, "missing ELF magic"
    if blob[4] != 2:
        return False, "not a 64-bit ELF"
    if blob[5] != 1:
        return False, "not little-endian"
    if blob[6] != 1:
        return False, f"unexpected ELF version {blob[6]}"

    (e_type, e_machine, _version, e_entry, e_phoff, e_shoff,
     _flags, _ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum,
     _shstrndx) = struct.unpack_from("<HHIQQQIHHHHHH", blob, 16)

    if e_type not in (ET_DYN, ET_EXEC):
        return False, f"unsupported ELF type {e_type}"
    if e_machine != EM_X86_64:
        return False, f"unexpected machine {e_machine}"
    if e_phnum == 0 or e_phentsize != PROGRAM_HEADER:
        return False, f"bad program header table (num={e_phnum}, size={e_phentsize})"
    if e_phoff + e_phnum * PROGRAM_HEADER > len(blob):
        return False, "program header table outside the file"

    loads = []
    for index in range(e_phnum):
        offset = e_phoff + index * PROGRAM_HEADER
        (p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz,
         _align) = struct.unpack_from("<IIQQQQQQ", blob, offset)
        if p_offset + p_filesz > len(blob):
            return False, f"segment {index} extends past end of file"
        if p_type == PT_LOAD:
            loads.append((p_vaddr, p_memsz))

    if not loads:
        return False, "no PT_LOAD segment"
    # These payloads are linked at vaddr 0 on purpose: the loader picks the
    # base address at load time, so a zero entry point is valid as long as it
    # falls inside a loadable segment.
    if not any(vaddr <= e_entry < vaddr + memsz for vaddr, memsz in loads):
        return False, "entry point is outside every PT_LOAD segment"

    if e_shnum:
        if e_shentsize != SECTION_HEADER:
            return False, f"bad section header size {e_shentsize}"
        if e_shoff + e_shnum * SECTION_HEADER > len(blob):
            return False, "section header table outside the file"
        for index in range(e_shnum):
            offset = e_shoff + index * SECTION_HEADER
            (_name, sh_type, _flags, _addr, sh_offset, sh_size) = struct.unpack_from(
                "<IIQQQQ", blob, offset)
            if sh_type == SHT_RELA and sh_offset + sh_size > len(blob):
                return False, f"relocation section {index} outside the file"

    return True, (
        f"ok: type={e_type} phdrs={e_phnum} loads={len(loads)} "
        f"entry=0x{e_entry:x} size={len(blob)}"
    )


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: validate_elf.py <payload.elf>", file=sys.stderr)
        return 2
    ok, reason = validate(sys.argv[1])
    print(f"{sys.argv[1]}: {reason}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
