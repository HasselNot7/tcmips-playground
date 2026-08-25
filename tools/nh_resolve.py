#!/usr/bin/env python3
"""Resolve addresses in tcmips_nethack.tcm.elf to function names.

Usage: python3 tools/nh_resolve.py <elf> <hexaddr> [<hexaddr> ...]
"""
import struct
import sys


def parse_symtab(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"\x7fELF", "not an ELF"
    ei_class, ei_data = data[4], data[5]
    assert ei_class == 1, "not 32-bit"
    endian = "<" if ei_data == 1 else ">"

    e_shoff = struct.unpack_from(endian + "I", data, 0x20)[0]
    e_shentsize = struct.unpack_from(endian + "H", data, 0x2E)[0]
    e_shnum = struct.unpack_from(endian + "H", data, 0x30)[0]
    e_shstrndx = struct.unpack_from(endian + "H", data, 0x32)[0]

    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        name, typ, flags, addr, offset, size, link, info, align, entsz = \
            struct.unpack_from(endian + "10I", data, off)
        sections.append(dict(name=name, type=typ, addr=addr, offset=offset,
                             size=size, link=link, entsz=entsz))

    shstr = sections[e_shstrndx]

    def sec_name(s):
        n = s["name"]
        end = data.index(b"\0", shstr["offset"] + n)
        return data[shstr["offset"] + n:end].decode()

    syms = []
    for s in sections:
        if s["type"] != 2:  # SHT_SYMTAB
            continue
        strtab = sections[s["link"]]
        count = s["size"] // 16
        for i in range(count):
            off = s["offset"] + i * 16
            st_name, st_value, st_size, st_info, _o, _shndx = \
                struct.unpack_from(endian + "IIIBBH", data, off)
            if st_name == 0 or st_value == 0:
                continue
            end = data.index(b"\0", strtab["offset"] + st_name)
            nm = data[strtab["offset"] + st_name:end].decode("utf-8",
                                                             "replace")
            syms.append((st_value, st_size, nm))
    syms.sort()
    return syms


def resolve(syms, addr):
    best = None
    lo, hi = 0, len(syms) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if syms[mid][0] <= addr:
            best = syms[mid]
            lo = mid + 1
        else:
            hi = mid - 1
    if not best:
        return "?"
    start, size, nm = best
    off = addr - start
    inside = "" if off < max(size, 1) else "+%#x??" % off
    return "%s+%#x%s" % (nm, off, inside)


def main():
    syms = parse_symtab(sys.argv[1])
    print("symbols:", len(syms))
    for a in sys.argv[2:]:
        v = int(a.replace("0x", ""), 16)
        print("%08x  %s" % (v, resolve(syms, v)))


if __name__ == "__main__":
    main()
