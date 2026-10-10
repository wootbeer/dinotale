# Status

The project's working log: every investigation and fix, newest first, as
"rounds". [PROGRESS.md](PROGRESS.md) has the short version of where things
stand; this file is for finding out why something is the way it is.

# Log

## Round 2: runtime project, overlay registration, audio microcode

**Runtime project.** CMake, frontend (`src/main/`) and crash reporting
adapted from the BattleTanx port, built against the same pinned
N64ModernRuntime/RecompFrontend/RT64. ROM registration: XXH3-64
`0xf1d8a6737dda5b82` of the `.z64`, entry point 0x80000400, 4 Kbit EEPROM
saves (the game's `eepmgr`). The game asks for the ROM on first launch, as
the BattleTanx port does.

**Overlay registration.** A `[[patches.hook]]` at the top of `Overlay_Load`
(0x8008169C) calls librecomp's `unload_overlays(ram, vramEnd - vramStart)`
then `load_overlays(vromStart, ram, vromEnd - vromStart)`, with `ram` being
the fifth argument (on the stack at sp+0x10). That sets the section's load
address, which the generated `RELOC_HI16/LO16` code uses, and maps each
overlay function's runtime address for indirect calls. `Overlay_Load` is the
only loader (its only caller is `Overlay_AllocateAndLoad`, whose only caller
is `func_80050A50`).

**UI pump.** recompui's per-frame callback pump (`recomp_run_ui_callbacks`)
is a hook just before the game's only `osViSwapBuffer` call (0x80071A18, in
the framebuffer swap loop `func_80071974`). The BattleTanx port used a MIPS
patch library for this, which needs a MIPS-capable clang (WSL on Windows);
a hook needs no extra toolchain. The hook saves and restores a0-a2 around
the pump. `patches/` keeps only the headers recompui includes.

**Loads into $zero.** Four `lw $zero, off(reg)` instructions (in
`func_80071FF0` and `func_8007C1A0`) come out of N64Recomp as `0 = MEM_W(...)`,
which doesn't compile. Their result is discarded, so they're patched to nops.

**Graphics microcode.** The ROM has F3DEX.NoN 1.23 and S2DEX 1.06 (their
version strings are at ROM 0xB4730 and 0xB41F9). RT64 emulates both itself.

**Audio microcode.** RSP code sits at the start of main's data, from ROM
0x92170: rspboot (0xD0 bytes), then the graphics microcode text, then the
audio text at ROM 0x94E60 (`D_80094260`, which the game DMAs with size
0x1000; the text proper is 0xF50 bytes, up to the next symbol). Its
command jump table is the 24 IMEM addresses at ROM 0xB4C90, the start of
the data block the decomp splits at 0xB4C80; those are RSPRecomp's extra
branch targets. It runs from IMEM 0x1000, not the usual 0x1080: at 0x1080
its jumps (`j 0x1054`, `jal 0x10C0`) land below its own start or in the
middle of a loop, while at 0x1000 they are the command fetch loop and the
DMA wait routine. `aspMain.us.toml` recompiles it to `rsp/aspMain.cpp`.

**App icon.** The project's chosen design, a white egg with three green
spots, drawn from shapes by `tools/make_app_icon.py`.

## Round 1: ROM layout, overlays, symbol file, first N64Recomp run

**ROM.** Yoshi's Story (USA) (En,Ja), SHA-1 of the `.z64`
`b13072fef6c6df48c07d8822c01e5bc59036f6da`, the same ROM the
[decomp](https://github.com/decompals/yoshis-story) targets: its build
reproduces it byte for byte. Common dumps are byte-swapped (`.v64` data
even when named `.n64`); `tools/normalize_rom.py` fixes that. The cartridge
uses CIC-6106, so the header's entry point (0x80200400) is 0x200000 above
the real one, 0x80000400.

**Layout.** From the decomp's splat config (`yamls/us/main.yaml`):

| ROM | vram | What |
|---|---|---|
| 0x001000-0x001070 | 0x80000400 | boot entry (hand-written asm) |
| 0x001070-0x092170 | 0x80000470 | main .text (game code + libultra) |
| 0x092170-0x0B58B0 | 0x80091570 | main .data / .rodata (bss follows to 0x80112CA0) |
| 0x0B58B0-0x0B6BCC0 | | data |
| 0x0B6BCC0-0x0EBDA70 | 0x80400000-0x807C6D00 | **308 relocatable overlays** |
| 0x0EBDA70-end | | assets |

**Overlays.** Unlike BattleTanx, most of this game's code is in
relocatable overlays, loaded at runtime by `Overlay_Load` and patched by
`DoRelocation` (the decomp's `loadfragment2.c`, the same scheme as Zelda 64).
The decomp's yaml doesn't split them yet (one `unkbin` after main), so:

- The game passes `{vromStart, vromEnd, vramStart, vramEnd}` descriptors to
  `Overlay_AllocateAndLoad` (via `func_80050A50`). Those descriptors live in
  asset data, not code, so a scan for them across the whole ROM found 304
  overlays.
- Each overlay ends with its own relocation section (text/data/rodata/bss
  sizes, relocation count and list, with the last word giving the offset back
  to that header). That lets the overlays be walked as one contiguous chain
  from 0xB6BCC0: **308 overlays**, each linked at its own unique vram,
  starting at 0x80400000 with each one following the previous one's bss.
  Every descriptor found agrees with the chain (0 mismatches); the 4 without
  a descriptor are 1 tiny overlay at the very start and 3 empty ones.
- No two overlays share a vram, so function addresses are unique across the
  whole game.

`tools/overlays.py` does this walk.

**Functions.** `tools/gen_overlay_yaml.py` writes splat segments for every
overlay (text/data/rodata/reloc/bss from each header), appended to the
decomp's own yamls. splat/spimdisasm then analyse main and all overlays
together: 10,829 functions. The decomp's ELF adds the libultra functions it
links from its own library build, with their real names, which matters
because N64Recomp swaps in its own implementations of libultra by name
(88 functions recognised, `osCreateThread`, `osPiStartDma`, the EEPROM
functions and so on).

**Relocations.** In N64Recomp's symbol file a section with a `relocs` list
is relocatable: the generated code addresses its own section through
`RELOC_HI16`/`RELOC_LO16` against wherever the runtime says that section is
loaded. Each overlay's text relocations (R_MIPS_26: 9,154, HI16: 6,777,
LO16: 6,775) come straight from its own relocation table, the same list the
game's `DoRelocation` applies, with HI16/LO16 paired by register exactly as
`DoRelocation` does. Data relocations (R_MIPS_32) stay with the game: the
recompiled `DoRelocation` still runs and patches the overlay's data in RDRAM.

**Result.** `N64Recomp yoshisstory.us.toml` exits cleanly on the first full
run: 217 C files, about 49 MB. `tools/regen_symbols.sh` reproduces the
symbol file from a ROM and a pinned decomp commit.

**Next.** The runtime project (adapted from the BattleTanx port), and
telling the runtime where each overlay was loaded: librecomp's
`load_overlays(rom, ram, size)` has to be called from the game's
`Overlay_Load`, or every call into an overlay will fail to resolve.
