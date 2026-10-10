#!/usr/bin/env python3
"""
Write splat segments for the relocatable overlays (see overlays.py).

The output replaces the decomp's yamls/us/overlays.yaml (which currently has a
single `unkbin` covering everything after main), so that splat/spimdisasm
analyse every overlay's code and give us function boundaries for it.

Usage: python3 tools/gen_overlay_yaml.py baserom.z64 > overlays.yaml
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import overlays  # noqa: E402

MAIN_ROM_END = 0xB58B0


def main():
    d = open(sys.argv[1], "rb").read()
    ovls = overlays.find_overlays(d)
    print("  - [0x%X, bin, unk_after_main]" % MAIN_ROM_END)
    for o in ovls:
        name = "ovl_%03d" % o["index"]
        print("  - name: %s" % name)
        print("    type: code")
        print("    start: 0x%X" % o["rom"])
        print("    vram: 0x%08X" % o["vram"])
        if o["bss_size"]:
            print("    bss_size: 0x%X" % o["bss_size"])
        print("    subsegments:")
        off = o["rom"]
        if o["text_size"]:
            print("      - [0x%X, asm, %s]" % (off, name))
        off += o["text_size"]
        if o["data_size"]:
            print("      - [0x%X, data, %s]" % (off, name))
        off += o["data_size"]
        if o["rodata_size"]:
            print("      - [0x%X, rodata, %s]" % (off, name))
        print("      - [0x%X, bin, %s_reloc]" % (o["reloc_header"], name))
        if o["bss_size"]:
            print("      - {type: bss, vram: 0x%08X, name: %s}"
                  % (o["vram"] + o["rom_end"] - o["rom"], name))
    print("  - [0x%X, bin, unk_after_overlays]" % ovls[-1]["rom_end"])
    print("  - [0x%X]" % len(d))


if __name__ == "__main__":
    main()
