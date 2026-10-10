#!/usr/bin/env python3
"""
Build N64Recomp's symbol file (YoshiStorySyms/yoshisstory.us.syms.toml).

Inputs, all derived from your own ROM:
  * the ROM (normalized .z64), for the overlay relocation tables
  * splat's `.splat/spim_context.csv`, from a splat run over the decomp's
    yamls plus tools/gen_overlay_yaml.py's overlay segments (function starts
    and sizes for main and every overlay)
  * optionally `nm` output of the decomp's ELF, which adds the libultra
    functions the decomp links from its own build of the library (splat
    doesn't disassemble those) and their real names

Output sections:
  * `main`: the boot segment (ROM 0x1000, vram 0x80000400), not relocatable
  * `ovl_NNN`: one per relocatable overlay, each with its text relocations
    (R_MIPS_26/HI16/LO16) taken straight from the overlay's own relocation
    table, i.e. exactly what the game's DoRelocation patches at load time.
    Data relocations (R_MIPS_32) are left to the game: the recompiled
    DoRelocation still applies them to the overlay's data in RDRAM.

Usage:
  python3 tools/make_syms.py baserom.z64 spim_context.csv [elf_nm.txt] > out.syms.toml
"""
import csv
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import overlays  # noqa: E402

MAIN_ROM = 0x1000
MAIN_VRAM = 0x80000400
MAIN_TEXT_END_ROM = 0x92170  # start of main's .data (decomp main.yaml)


def load_spim_functions(path):
    funcs = {}
    for r in csv.DictReader(open(path)):
        if r["getType"] != "@function" or r["sectionType"] != ".text":
            continue
        vram = int(r["address"], 16)
        size = int(r["getSize"], 16) if r["getSize"] not in ("None", "") else None
        funcs[vram] = [r["getName"], size]
    return funcs


def _is_func_name(name):
    # Skip data labels and the linker script's segment-boundary symbols
    # (main_VRAM, main_TEXT_START, ...), which nm also reports as text.
    if name.startswith(("D_", ".", "jtbl_", "L8")):
        return False
    return not any(tag in name for tag in ("_VRAM", "_ROM_", "_TEXT_", "_DATA_", "_RODATA_", "_BSS_"))


def load_nm_functions(path):
    funcs = {}
    for line in open(path):
        parts = line.split()
        if len(parts) == 4 and parts[2] in ("T", "t") and _is_func_name(parts[3]):
            funcs[int(parts[0], 16)] = [parts[3], int(parts[1], 16)]
        elif len(parts) == 3 and parts[1] in ("T", "t") and _is_func_name(parts[2]):
            funcs[int(parts[0], 16)] = [parts[2], None]
    return funcs


def sized(starts, text_start, text_end, given):
    """Give every function a size reaching to the next function start (or
    the end of text). Splat's sizes stop at the last instruction, leaving
    alignment padding unowned; covering it is harmless and keeps the section
    contiguous."""
    out = []
    addrs = sorted(a for a in starts if text_start <= a < text_end)
    for i, a in enumerate(addrs):
        end = addrs[i + 1] if i + 1 < len(addrs) else text_end
        out.append((given[a][0], a, end - a))
    return out


def hi_lo_targets(d, o):
    """Resolve each text HI16/LO16/26 reloc to its target vram the way the
    game's DoRelocation pairs them (by register)."""
    base_rom, base_vram = o["rom"], o["vram"]
    sec_off = {"text": 0, "data": o["text_size"], "rodata": o["text_size"] + o["data_size"]}
    out = []
    lui = {}
    for sec, typ, off in o["relocs"]:
        if sec != "text":
            continue
        addr = base_vram + sec_off[sec] + off
        insn = struct.unpack_from(">I", d, base_rom + sec_off[sec] + off)[0]
        if typ == "R_MIPS_26":
            target = 0x80000000 | ((insn & 0x03FFFFFF) << 2)
            out.append((addr, "R_MIPS_26", target))
        elif typ == "R_MIPS_HI16":
            lui[(insn >> 16) & 0x1F] = (addr, insn)
            out.append([addr, "R_MIPS_HI16", None])
        elif typ == "R_MIPS_LO16":
            hi_addr, hi_insn = lui[(insn >> 21) & 0x1F]
            lo = struct.unpack(">h", struct.pack(">H", insn & 0xFFFF))[0]
            target = ((hi_insn & 0xFFFF) << 16) + lo
            target &= 0xFFFFFFFF
            out.append((addr, "R_MIPS_LO16", target))
            for r in out:
                if isinstance(r, list) and r[0] == hi_addr and r[2] is None:
                    r[2] = target
    for r in out:
        if r[2] is None:
            raise ValueError("unpaired HI16 at 0x%08X" % r[0])
    # N64Recomp walks a section's relocs in step with its instructions, so
    # they must be in address order; the game's tables aren't always (an
    # IDO HI16 can be listed after a later one).
    return sorted((tuple(r) for r in out), key=lambda r: r[0])


def emit_section(name, rom, vram, size, funcs, relocs=None):
    lines = ["[[section]]", 'name = "%s"' % name, "rom = 0x%08X" % rom,
             "vram = 0x%08X" % vram, "size = 0x%X" % size, "", "functions = ["]
    for fname, fvram, fsize in funcs:
        lines.append('    { name = "%s", vram = 0x%08X, size = 0x%X },' % (fname, fvram, fsize))
    lines.append("]")
    if relocs is not None:
        lines += ["", "relocs = ["]
        for addr, typ, target in relocs:
            lines.append('    { type = "%s", vram = 0x%08X, target_vram = 0x%08X },' % (typ, addr, target))
        lines.append("]")
    lines.append("")
    return "\n".join(lines)


def main():
    d = open(sys.argv[1], "rb").read()
    funcs = load_spim_functions(sys.argv[2])
    if len(sys.argv) > 3:
        for a, v in load_nm_functions(sys.argv[3]).items():
            if a in funcs:
                funcs[a][0] = v[0]  # the decomp ELF has the better name
            else:
                funcs[a] = v
    out = ["# Generated by tools/make_syms.py; see BUILDING.md and STATUS.md.", ""]
    main_text_end = MAIN_VRAM + MAIN_TEXT_END_ROM - MAIN_ROM
    # The boot entry (makerom's hand-written `entry`, 0x80000400-0x80000470)
    # isn't a C function, so splat doesn't report it as one.
    funcs.setdefault(MAIN_VRAM, ["entrypoint", None])
    main_funcs = sized(funcs, MAIN_VRAM, main_text_end, funcs)
    out.append(emit_section("main", MAIN_ROM, MAIN_VRAM, MAIN_TEXT_END_ROM - MAIN_ROM, main_funcs))
    total = len(main_funcs)
    for o in overlays.find_overlays(d):
        text_end = o["vram"] + o["text_size"]
        if o["text_size"] == 0:
            continue
        starts = dict(funcs)
        starts.setdefault(o["vram"], ["func_%08X" % o["vram"], None])
        ofuncs = sized(starts, o["vram"], text_end, starts)
        total += len(ofuncs)
        out.append(emit_section("ovl_%03d" % o["index"], o["rom"], o["vram"], o["text_size"],
                                ofuncs, hi_lo_targets(d, o)))
    sys.stdout.write("\n".join(out))
    sys.stderr.write("%d functions\n" % total)


if __name__ == "__main__":
    main()
