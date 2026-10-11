# Status

The project's working log: every investigation and fix, newest first, as
"rounds". [PROGRESS.md](PROGRESS.md) has the short version of where things
stand; this file is for finding out why something is the way it is.

# Log

## Round 14: junk on the top/left edge

**Report:** in levels the top row and left column of the screen show
coloured junk that changes, rather than looking like part of the storybook.

**Cause:** in levels the game never draws those pixels. The sky layer starts
one pixel in from the top-left, and the layers in front of it have
transparent areas there, so the framebuffer keeps whatever was left in it.
On a TV they were in the overscan.

**Fix** (`rt64/0008`): the final output masks the top row and left column
to black.

**Expand, at a level's left end:** the extra width there reaches past the
level's start, where the game never streams background columns, so those
buffer columns still hold whatever an earlier scroll left. That goes away
once the camera limits account for the wider view (next).

## Round 13: Expand, first part of true widescreen (backgrounds)

**Background buffers.** `func_8004F2B0` (bgManager) creates each level's
background view with a width and height from the level data (320x238) and
passes them to `func_8004EEEC`. That allocates the layer's ring buffer as
(width + 16) x (height + 18) and records the view size, which everything
downstream uses: the camera window (`func_8004F44C`: centre +/- size/2), the
tile streamer, the ring wrap (`func_800734F0`), and the `uObjBg` that
`func_8007D5E0` builds (frame width = view width, frame x = centre - width/2).
A hook just before the call (`yoshisstory.us.toml`) widens the view in Expand
(`src/game/widescreen.cpp`: the window's aspect, capped at 16:9 for now, in
whole 16-pixel columns, so 432 for 16:9). The game then streams and draws a
432-pixel view centred on the original 320 one; Original and the other modes
pass the width through unchanged.

**RT64** (`rt64/0007`), in Expand:

- A scissor over the whole 320-wide screen is anchored to the window edges
  with extended-GBI origins, and `G_BG_1CYC` no longer cuts the frame to the
  4:3 scissor horizontally.
- Backgrounds keep their shape (`G_EX_ASPECT_ADJUST`) instead of being
  stretched as full-screen rectangles.
- The extended-origin aspect is forced to the full window, since
  the widened scissor depends on it.

**Result:** all three parallax layers fill a 16:9 window with real level
scenery. Still to do: sprites and enemies are only drawn inside the 4:3 area
(the game culls them at the old screen edges), and at a level's left end the
far layers show a strip of black because the camera limit is still the 4:3
one.

## Round 12: text box strips (S2DEX SHRINKSIZE)

**Report:** lines flicker in the text of message boxes while it scrolls.

**Findings.** A message box's text is one 256x82 4-bit intensity image that
the game redraws as the text scrolls. The lines entering and leaving at the
edges are squashed on purpose, like text on a rolling drum. The image goes to
the screen as several `G_OBJ_RECTANGLE_R` strips of 32 rows (one TMEM load
each), scaled to about 0.7-0.85x, with bilinear filtering and
`G_OBJRM_SHRINKSIZE_1`. The strips overlap by one texel row. SHRINKSIZE trims
half a texel off each edge of each strip so that bilinear filtering never
blends with texels outside the strip, which makes the joins seamless. RT64's
new object code ignored it, so each join drew a one-pixel band whose blend
was wrong, at a fixed height, with the text scrolling through it.

**Fix** (`rt64/0006`): `G_OBJRM_SHRINKSIZE_1`/`_2` inset the sprite by
half a texel / one texel on every side, in both the screen rectangle and the
texture coordinates.

**Top line in levels:** on the reporter's machine the top screen row in
levels shows coloured pixels instead of black. Each background layer is a
ring buffer that also wraps vertically, and the sky layer starts one pixel
down, so the row is at the very edge of what the game draws. It isn't
reproduced here at 1x or 3x yet.

## Round 11: aspect ratio options (Stretch, Hybrid) and patch handling

**Aspect ratio options.** Settings > Graphics > Aspect Ratio now offers:

- **Original:** 4:3 with black bars (the default again; it was Expand).
- **Stretch:** the original 4:3 picture, menus and all, scaled to fill the
  window. RT64 renders at 4:3 and the final presentation scales each axis
  separately (`stretchToWindow`).
- **Hybrid:** RT64's widened mode, with every S2DEX background stretched
  to the full width while sprites keep their shape. Before this only
  backgrounds that covered the whole scissor were stretched, and the sky
  layer, which the game draws one pixel in from the edge, stayed 4:3 and left
  black corners. Sprites are placed in 4:3 terms, so they shift slightly
  against the scenery towards the sides.
- **Expand** is kept for true widescreen (more play area, round 10's plan)
  and hidden until it works; a config that already says Expand behaves like
  Hybrid for now.

Patches: `rt64/0005`, `N64ModernRuntime/0002`, `RecompFrontend/0001` (aspect ratio options).

**The thin line at the top/left edge** in levels is the original game: the
sky layer starts one pixel in, so the layer behind shows through. The
reference emulator shows the same; on a TV it was in the overscan.

**Patch handling.** Configure used to test each patch on its own by
reverse-applying it. Once a later patch changed lines next to an earlier one,
the earlier patch no longer reversed cleanly, looked unapplied, and configure
failed trying to apply it again. Now the applied set is recorded by hash in
the submodule's git directory; when the set changes, the submodule's files are
reset and all of its patches applied again in order.

## Round 10: widescreen feasibility

**Question:** can the levels show more play area in 16:9, rather than
stretching? RT64's "Expand" mode currently stretches the two front background
layers (they span the full 4:3 width) and leaves the sky layer at 4:3, so the
top corners are black and sprites no longer line up with the scenery.

**How the level backgrounds work** (traced from the machine code with memory
diffs and watchpoints; the decomp hasn't reached this code yet):

- Each of the three parallax layers is a 336x256 CI8 image used as a
  horizontal **ring buffer** of 21 columns of 16 pixels. S2DEX's
  `G_BG_1CYC` wraps the image, so the view can start at any column.
- As the camera moves, the game draws the newly exposed 16x16 tiles into the
  ring buffer **with the RDP**: it points the colour image at the buffer and
  draws tile rectangles. Only 16 pixels exist beyond the 320-pixel screen.
- The code lives in `main` around 0x8005EC98-0x8005F700: `func_8005EC98`
  copies each frame's tile requests to the layer, `func_8005F5D0` sets the
  render target (`func_80073728`), draws the queued tiles
  (`func_8005F3F4`/`func_8005F510` -> `func_80073D58`) and then the
  background itself (`func_8007DB34`).
- The buffers are allocated from the game's heap. The game uses only the
  first 4 MB of RDRAM; the 4 MB above are free.

**Verdict: feasible.** The ring buffer's size is data (width, height, column
count) rather than being baked into the drawing code, and there is room in
memory for wider buffers. The plan: widen the buffers and the column count,
make the tile queue cover the wider view, draw the backgrounds across the
full width with RT64's extended GBI, then widen sprite/enemy culling and the
camera limits. Still to find: where the 336/21 values and the tile queue's
visible width are set.

## Round 9: black level backgrounds and garbled tiles

**Symptoms:** in levels the parallax backgrounds were black (so moving
sprites left trails), and the dotted ring and some block tiles were garbled.
Two separate RT64 problems, both fixed with new patches in
`lib-patches/rt64/`.

**Garbled tiles: S2DEX status words** (`0003-s2dex-status-words.patch`).
S2DEX's `G_OBJ_LOADTXTR` family skips a texture load when a status word
already says that texture is in TMEM. Two things were wrong in RT64:
`gSPSetStatus` (`G_MOVEWORD` with `G_MW_GENSTAT`) was an `assert(false)`
no-op, and the status words survived when the microcode was reloaded. On
the RSP they live in the microcode's DMEM data, which a task start or a
`G_LOAD_UCODE` overwrites; Yoshi's Story switches between S2DEX and F3DEX in
the middle of every frame, so stale words made S2DEX skip loads of textures
F3DEX had since replaced. Now `G_MW_GENSTAT` sets the word, and the S2DEX
state is reset on every microcode load as well as every task.

**Black backgrounds: 8-bit framebuffers**
(`0004-ci8-framebuffer-reads-from-rdram.patch`). Each level background
layer is a 336x256 CI8 image in RDRAM, drawn every frame with
`G_BG_1CYC`. At level start the game also points the RDP's colour image at
those images (8-bit, colour-index) and draws pieces of them with the RDP;
the rest comes from the CPU. RT64 then tracks those images as framebuffers,
and when `G_BG_1CYC` reads them it takes its "framebuffer tile copy" path,
copying from the GPU's version of the framebuffer instead of RDRAM. A GPU
framebuffer can't hold the colour indices (and only had the RDP-drawn
pieces anyway), so the result was black. RT64 now never uses a tile copy
from an 8-bit framebuffer and reads those textures from RDRAM, as the
hardware does. Checked against the reference emulator: the first level's
background matches.

Two earlier experiments turned out not to be needed and were dropped
(never discarding framebuffers the CPU changed, keeping the viewport across
tasks).

## Round 8: blank Options screen -- RT64 was missing S2DEX sprite commands

**Report (Windows, real GPU):** the menus work, but choosing Options gives a
blank screen while the sound carries on. Reproduced on the Linux test setup.

**Not the game.** The game kept running normally (about 30 buffer swaps a
second). To check whether its logic had gone wrong, the original game was
run in an emulator to the same screen and the two RDRAMs compared:
- the boot-check values the game stored (`D_80108620` etc., `osCicId`, the
  RDRAM words) are identical to the emulator's, so round 3's answers are
  right;
- the same overlays are resident at the same addresses, and the Options
  screen's overlays (ROM 0xC622B0, 0xE15C80, 0xEAB730, 0xEAF270) match the
  emulator word for word, code, data and bss;
- the display lists for the frame are identical, command for command.

**RT64.** The Options frame switches to the S2DEX 1.06 microcode
(`G_LOAD_UCODE`, recognised by RT64: its text and data hashes match RT64's
database) and draws with `G_OBJ_MOVEMEM`, `G_OBJ_LDTX_SPRITE` and
`G_OBJ_LDTX_RECT_R`. RT64's S2DEX implementation (upstream too, as of this
round) only had backgrounds and texture loads: the load-and-draw commands
loaded the texture and stopped at a `TODO` / `assert(false)`, and
`G_OBJ_RECTANGLE`, `G_OBJ_SPRITE`, `G_OBJ_RECTANGLE_R` and `G_OBJ_MOVEMEM`
had no handlers. So nothing drawn with sprites appeared, here and in the
levels.

**Fix:** `lib-patches/rt64/0002-s2dex-object-sprites.patch` implements them:
- `G_OBJ_MOVEMEM` loads the 2D matrix (`uObjMtx`, or `uObjSubMtx` for X, Y
  and base scale);
- `G_OBJ_RECTANGLE` draws a sprite at its own position and scale;
- `G_OBJ_RECTANGLE_R` positions and scales it with the matrix's X/Y and base
  scale;
- `G_OBJ_SPRITE` applies the full matrix. It's drawn as a texture rectangle,
  so scaling, mirroring (the game's matrix has D = -1) and translation are
  right; rotation/shear are approximated by the bounding box for now;
- the `G_OBJ_LDTX_*` commands now draw after loading.
Each sets up the render tile from the `uObjSprite` the way the microcode
does (clamped unless `G_OBJRM_NOTXCLAMP`) and draws a texture rectangle.

**Result:** the Options chalkboard renders as in the emulator, the "Reveal
Lucky Fruit" screen works, and levels now show Yoshi, fruit, coins, enemies
and the HUD.

**Still wrong in levels:** the background is black (and because nothing
covers the frame, moving sprites leave trails), and the block tiles around
the level show garbled textures. Next round.

**Tooling:** a mupen64plus reference run (keyboard input works once its SDL
input plugin is configured with `--set Input-SDL-Control1[...]`), with
savestates read as RDRAM snapshots, and a gdb dump of the port's RDRAM at the
same point.

## Round 7: Visual Studio didn't offer DinoTale.exe as a startup item

The command-line build worked, but Visual Studio's green play button had no
DinoTale.exe. Visual Studio configures the preset itself, in its own
environment, and the presets named the compiler only as `clang-cl`, which
that environment doesn't reliably put on PATH (the setup script does it
itself, which is why its build worked). With the configure failing, Visual
Studio has no targets to offer. The presets now give the full path,
`$env{VCINSTALLDIR}Tools/Llvm/x64/bin/clang-cl.exe`; `VCINSTALLDIR` is set
in Visual Studio's own environment and in the developer shell the setup
script starts. The same path also avoids the 32-bit-hosted clang-cl that
caused trouble in the BattleTanx port.

## Round 6: Visual Studio setup

For building and debugging locally on Windows:

- `CMakePresets.json`: **Windows Release** and **Windows Debug**
  (RelWithDebInfo) presets with clang-cl and Ninja, which Visual Studio's
  Open Folder picks up, plus a Linux one.
- `tools/setup-windows.ps1`: one-time setup. It enters Visual Studio's x64
  environment itself, fetches submodules, converts the ROM's byte order (in
  C#, no Python needed) and checks its SHA-1, builds and runs N64Recomp,
  then configures and builds the Release preset. `-Icon` rebuilds the app
  icon files from a PNG.
- The build copies `assets/` next to the exe, and the game switches to its
  own folder at startup when the working directory has no `assets/`, so it
  runs from Visual Studio's F5 or a double-click as well as from the repo
  root.

## Round 5: first level runs; Windows build path

**The game reaches its first level.** From the Page 1 book Yoshi goes into
the level and is controllable (he jumps on A). The game's frame loop and
RSP tasks keep running normally there (about 35 buffer swaps a second
under the debugger).

**What isn't known yet: the level's look.** On the Linux test setup
(software Vulkan on a virtual X server) the level shows black around Yoshi,
and Yoshi leaves trails, as if nothing clears or covers the frame. But that
setup also stops presenting partway into the level: the software driver's
presenter blocks sending the image to the virtual X server while the game
keeps rendering, so the screenshots there stop updating. Whether the
missing background is the port or the test setup needs a real GPU. The
next test is a Windows build.

**Ruled out:** the RSP. Logging every distinct RSP task shows only the
audio microcode (type 2, ucode 0x80094260, data 0x800B4080) besides
graphics, which RT64 handles; the game doesn't use the RSP for anything
else such as decompression.

**Windows.** `BUILDING.md` now has the Windows steps (Visual Studio 2022
with clang-cl, as for the BattleTanx port, minus WSL: there's no MIPS
patch library here) and `tools/package-windows.ps1` packages a release zip.

**Diagnostics.** RSP tasks are logged once per distinct microcode
(`[RSP] task type ...` on stderr).

## Round 4: menus and Story Mode; overlays reused in memory

**Input works.** With the frontend's default keyboard binding (Enter =
Start) the game goes from the intro to the title book, the main menu (Story
Mode / Trial Mode / Options / Practice) and into Story Mode's opening scene.

**Overlays loaded more than once.** The game frees an overlay's memory and
later loads overlays into the same memory, and loads the same overlay again
at a different address (the overlay at ROM 0xBBFF30 was first at 0x803FF420,
later at 0x803F98C0). The runtime isn't told when an overlay is freed, so it
only learns at the next `Overlay_Load` that memory is being reused, and
that region doesn't line up with the old overlay: librecomp's
`unload_overlays` stopped the game with "Cannot partially unload section".
A runtime patch (`lib-patches/N64ModernRuntime/`, applied at configure time
like the RT64 one) changes two things:

- an overlay that's only partly covered by reused memory is dropped whole;
- dropping a stale copy doesn't reset the section's address if the same
  overlay has since been loaded again elsewhere. Without this, loading a
  new overlay over the old copy's memory pointed the live copy's code back
  at its link-time address, and it read a null pointer from what it took to
  be its own data.

**Now:** in Story Mode's first level Yoshi is drawn but the level around
him is black; next round looks at why.

## Round 3: first boot -- title screen and intro

On Linux with a software Vulkan renderer (lavapipe) the game now boots to
the title screen and plays the storybook intro, drawn by RT64. What it took,
in the order the crashes came:

**Hardware reads in the boot thread.** `Idle_ThreadEntry` stores the first
words of SP IMEM and DMEM (`IO_READ(0xA4001000)`, `IO_READ(0xA4000000)`),
which aren't memory here.

**Boot-state checks.** Those two values turned out to be part of a set of
checks on what the console leaves behind after booting an original
cartridge. Each stores a flag, and a failed one breaks the game quietly
later rather than at once:

| Read | Expected | Flag | On failure |
|---|---|---|---|
| SP IMEM[0] | 0x17D8 | `D_800A5AD0` | |
| SP DMEM[0] | 0xFFFFFFFF | (checked in `func_80051504`) | object setup is skipped |
| `osCicId` (0x80000310) | 0x17D8 | `D_800A6EB4` | a memory block is never freed |
| RDRAM 0xA0000164 | 0x01EC6021 | `D_800A6EB8` | a callback becomes a stub returning 0 |
| RDRAM 0xA00002B8 | 0x8941680C | `D_800A62F0` | |
| ROM 0x458/0x45C over the PI bus (`func_8006F500`) | 0x0260BCD5 | `D_800A6590` | |

In each case the flag is 1 when the value matches, and the matching value
is what an original cartridge gives (the ROM check compares against the
ROM's own bytes). They're all answered that way: the IMEM/DMEM loads become
immediates, the uncached RDRAM reads go through KSEG0 instead and the words
are seeded at boot, and `func_8006F500` is replaced by a hook that stores
its result.

**Audio interface.** `func_8000AD10` is the game's own copy of
`osAiSetNextBuffer`, writing the AI registers directly. A hook hands it to
the runtime's version.

**Uncached buffers.** `func_800035B4`/`func_800035F8` return freshly
allocated buffers as KSEG1 addresses (+0x20000000). The runtime maps RDRAM
only at KSEG0, so the add is removed; nothing converts them back.

**EEPROM size.** The game creates its EEPROM manager with type 2 and hangs
(`eepmgr_Probe`'s `LOOP()`) if the probe answers anything else, so the save
type is 16 Kbit, not 4 Kbit as first set in round 2.

**Relocation order.** The first overlay function to use a table in its own
data called through a null pointer: of two `lui`s next to each other, only
one had been relocated. The game's relocation tables aren't always in
address order (here the HI16 for 0x80455814 comes before the one for
0x80455810), and N64Recomp steps through a section's relocations in order
with its instructions, so it skipped the second. `tools/make_syms.py` now
sorts them; 711 relocations had been out of order.

**Crash reports** now translate faulting addresses to N64 addresses (the
crash handler is told where RDRAM is from the `Idle_ThreadEntry` hook).

**Overlays seen loading before the intro:** 13, all registered and
relocated as designed in round 2.

**Testing setup.** Xvfb plus lavapipe, with the ROM pre-stored in a
portable folder and `--game yoshisstory` to skip the launcher.

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
