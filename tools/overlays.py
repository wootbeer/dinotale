#!/usr/bin/env python3
"""
Find Yoshi's Story's relocatable code overlays in a ROM.

The game loads most of its code as relocatable overlays (the decomp's
loadfragment2.c: Overlay_Load / DoRelocation). In the US ROM they sit back to
back from 0xB6BCC0 to 0xEBDA70, and each one ends with its own relocation
section:

    textSize, dataSize, rodataSize, bssSize, nRelocations, relocs[n], ..., offset

where the last word of the overlay is the offset from the end of the file back
to that header. Each overlay was linked at its own unique "fake" vram
(0x80400000 upwards), with the next one starting where the previous one's bss
ends. The game's own overlay descriptors ({vromStart, vromEnd, vramStart,
vramEnd}, scattered through the asset data) agree with that layout for every
overlay that has one, which this script checks.

Usage (as a module): overlays.find_overlays(rom_bytes) -> list of dicts
Usage (CLI):         python3 tools/overlays.py baserom.z64
"""
import struct
import sys

CHAIN_ROM_START = 0xB6BCC0
CHAIN_VRAM_START = 0x80400000

RELOC_TYPE_NAMES = {2: "R_MIPS_32", 4: "R_MIPS_26", 5: "R_MIPS_HI16", 6: "R_MIPS_LO16"}
RELOC_SECTIONS = {1: "text", 2: "data", 3: "rodata"}


def u32(d, off):
    return struct.unpack_from(">I", d, off)[0]


def _descriptors(d):
    """Map vromStart -> set of (vromEnd, vramStart, vramEnd) for every
    plausible overlay descriptor in the ROM."""
    known = {}
    for off in range(0, len(d) - 16, 4):
        a, b, c, e = struct.unpack_from(">4I", d, off)
        if (0x1000 <= a < b <= len(d) and 0x80000000 <= c < e < 0xA0000000
                and 0x10 <= b - a < 0x100000 and 0 <= (e - c) - (b - a) < 0x40000):
            known.setdefault(a, set()).add((b, c, e))
    return known


def _header(d, end):
    back = u32(d, end - 4)
    h = end - back
    ts, ds, rs, bs, n = struct.unpack_from(">5I", d, h)
    return h, ts, ds, rs, bs, n


def _find_end(d, start):
    """Find the end of the overlay starting at `start` by looking for the
    first footer whose header describes exactly [start, header)."""
    for cand in range(start + 0x20, min(start + 0x100000, len(d)) + 1, 0x10):
        back = u32(d, cand - 4)
        if 0 < back <= cand - start:
            h = cand - back
            ts, ds, rs, bs, n = struct.unpack_from(">5I", d, h)
            if h == start + ts + ds + rs and 20 + 4 * n + 4 <= back <= 20 + 4 * n + 4 + 0x10:
                return cand
    return None


def find_overlays(d):
    known = _descriptors(d)
    out = []
    start, vram = CHAIN_ROM_START, CHAIN_VRAM_START
    while True:
        end = None
        if start in known and len(known[start]) == 1:
            b, c, e = next(iter(known[start]))
            if c != vram:
                raise ValueError("overlay at rom 0x%X: chain says vram 0x%08X, descriptor says 0x%08X"
                                 % (start, vram, c))
            end = b
        else:
            end = _find_end(d, start)
        if end is None:
            break
        h, ts, ds, rs, bs, n = _header(d, end)
        relocs = []
        for r in struct.unpack_from(">%dI" % n, d, h + 20):
            relocs.append((RELOC_SECTIONS[r >> 30], RELOC_TYPE_NAMES[(r >> 24) & 0x3F], r & 0xFFFFFF))
        out.append(dict(index=len(out), rom=start, rom_end=end, vram=vram, text_size=ts,
                        data_size=ds, rodata_size=rs, bss_size=bs, reloc_header=h,
                        relocs=relocs, has_descriptor=start in known))
        vram += (end - start) + bs
        start = end
    return out


def main():
    d = open(sys.argv[1], "rb").read()
    ovls = find_overlays(d)
    for o in ovls:
        print("ovl_%03d rom %06X-%06X vram %08X text %5X data %5X rodata %5X bss %5X relocs %4d%s"
              % (o["index"], o["rom"], o["rom_end"], o["vram"], o["text_size"], o["data_size"],
                 o["rodata_size"], o["bss_size"], len(o["relocs"]),
                 "" if o["has_descriptor"] else "  (no descriptor)"))
    print("%d overlays, rom %06X-%06X" % (len(ovls), ovls[0]["rom"], ovls[-1]["rom_end"]))


if __name__ == "__main__":
    main()
