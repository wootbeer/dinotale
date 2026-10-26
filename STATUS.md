# Status

The project's working log: every investigation and fix, newest first, as
"rounds". [PROGRESS.md](PROGRESS.md) has the short version of where things
stand; this file is for finding out why something is the way it is.

# Log

## Open: 4-1's blue blocks (issue #28)

**Report:** in 4-1's treehouses (the long row near the end) the blue blocks
wiggle as Yoshi walks and vanish at the edge of the 4:3 area in Expand.

**From the reporter's clip:** the blocks move in whole N64 pixels (4.25 and
8.5 px per frame at 1080p) while the wall behind moves in quarter pixels (3.25,
6.75): the same sprite-vs-background mismatch as #16, parked with it. The
rightmost block vanishes when its centre passes about x = 368 (320 + 48) in the
320-wide view's coordinates, before the wider view's edge (~373).

**Not yet found:** the check. Not the shared test (func_800591EC, already
widened), nor func_80022448 (which tests -48 < x < 368, but only to smash the
blocks on screen), nor the model draw functions' depth test (func_8005DCB4).
Candidates left: direct screen-x tests in overlays (func_8041F6BC,
func_8041F804, func_806AE81C, func_8059FFE0, func_80409618) and a float test in
an overlay. Needs the blocks on screen to see which runs; the treehouses are
far into the level.

## Round 49: stilts still missing in the left strip (issue #19)

**Report:** after Round 41 the stilts show in the right strip but not the left.

**Cause:** the shared on-screen test, func_800591EC, is strict on both sides,
-margin < x < 320 + margin, so with no margin the on-screen columns are
1..319. `dinotale_actor_screen_x` mapped the left strip to column 0, which
fails with no margin; the right strip maps to 319, which passes. The stilt
pieces are actors func_805175E4 spawns, tested with no margin.

**Fix:** the left strip maps to column 1 (the leftmost visible column, -extra,
still maps to 0 and is off screen, as column 0 is on the N64).

## Round 48: chalkboards' green stops short of the bottom frame (issue #25)

**Report:** on the results chalkboard (and message boxes) the green doesn't
reach the bottom wooden frame; a black band shows between them. Reproduced on
the Options chalkboard.

**Cause:** the board's green is one 16x16 texture the game stretches about
12 times over the whole board (a G_OBJ_SPRITE scaled to 241x193), drawn with
G_OBJRM_SHRINKSIZE_1. RT64 (`rt64/0006`) trimmed half a texel off each edge
in screen pixels: here 6 pixels. The frame hides it at the top and sides, but
the bottom bar's top rows are empty (the same flipped image as the top bar,
whose shadow falls on the green), so the green ended at y=203.75 and the wood
started at 208.7.

**Fix** (`rt64/0023`): a sprite drawn larger than its texture loses at most
the trim in screen pixels (half a pixel), and its texture starts that far in;
that's as far as bilinear filtering reaches. Sprites drawn at about their own
size (strips, text boxes, clouds) are unchanged. The Options board's green now
reaches the frame.

## Round 47: hills stepping up and down on a jump (issue #16)

**Report:** since Round 43, 1-1's tall hills wiggle up and down when Yoshi
jumps; not in 0.1.4 (where their right edges wiggled a little instead).

**Cause:** the hills are large strip images (256x8 strips, drawn with
G_OBJRM_SHRINKSIZE), which `rt64/0021` puts on whole pixels, vertically too.
As the camera follows a jump their sub-pixel height changes and they stepped
a whole pixel at a time while everything else moved smoothly.

**Fix:** `rt64/0021` keeps the strips' top and bottom where the game puts
them. Strips of one image still meet exactly (the bottom of one is the top of
the next), so no lines; checked at 3x mid-jump. Horizontally nothing changes
(the width stays fixed, which calmed the fence).

**Round 47b:** the hills stopped stepping, but the cardboard clouds then
wobbled up and down on a jump. They're drawn scaled (16-row strips drawn 15.55
px tall), and at a fraction of a pixel their rows land on different pixels as
they move. The hills are drawn at their own height (8-row strips, 7 px after
the half-texel trim each side). So only images drawn at their own height keep
exact edges; scaled ones are rounded as before.

## Round 46: the 4-4 driver crash is back (issue #17)

**Report:** on 0.1.4 (with Round 40's 8 MB stacks), 4-4 crashed twice more.
Same log: access violation at offset 0x7482ee in `nvwgf2umx.dll` on the Gfx
Thread, writing 0x20 below a 1 MB-aligned address.

**So:** the bigger stacks didn't change it; the stack theory was probably
wrong. The crash is inside NVIDIA's D3D12 driver (`nvwgf2umx.dll`), at one
instruction every time. Not reproducible here (no NVIDIA GPU).

**Change:** the Graphics API option (Auto / D3D12 / Vulkan), which the
frontend hides, is shown in the graphics settings, so a player hitting this
can switch to Vulkan, which doesn't go through that DLL (`RecompFrontend/0002`;
it applies at the next start). Whether Vulkan avoids the crash tells us if
it's D3D12-specific.

**Later (issue #27):** the same crash in 5-1, after 0.1.5, at exactly the same
host address (`0x23fc49fffe0`) as one of the 4-4 logs. The same address in
separate runs points to a fixed buffer the driver writes past, not a stack.
4-1, 4-4 and 5-1 all have full-screen water effects. Waiting on a Vulkan test.

## Round 45: flashing strip at the right edge after a teleport (Expand)

**Report:** after the page-turn of 1-1's first reachable teleport, the
rightmost few pixels flash; Expand only.

**Diagnosis:** a temporary log (`DINOTALE_BG_LOG`) showed the background
layers' frames and scissors unchanged after the teleport (432 px wide from
-56, covering the wider view), and the clip showed the strip holding pieces
of the curling page, different in each of the three screen buffers: nothing
was drawn there. A background layer is a ring buffer of 16-pixel tile
columns the game streams in as the camera moves. A full refill
(func_8004FA64: a level's start, a teleport or area change) asks for the
view's width in columns from the view's left column, without the extra
column the row request (func_8004FB98) adds when the view's left edge isn't
on a tile boundary. Expand's view starts 56 px left of the 4:3 one, half a
tile off, so the refill (27 columns) stopped 8 px short of the layer's right
edge; with every layer empty there, the screen showed what was left in it.

**Fix:** a hook in func_8004FA64 adds the column when the view's left edge
isn't on a tile boundary (28 columns, the ring buffer's width).

## Round 44: screen-copy effects in Expand (issues #9, #11, #21)

**Reports:** in Expand, 4-2's water reflection and 2-3's goo stop at the 4:3
edge; the page-turn transition (pots, area changes, the jump-on teleporters)
shows one page at 4:3 inside the wider one.

**Cause:** these effects draw geometry (orthographic triangles) textured with
a copy of the screen the game just drew (4-2: 4-row strips from x -160 to
160, sampling the 320-wide framebuffer). In Expand RT64's render target is
wider, and the copy's native coordinates (0..320) cover the whole widened
target; but the triangles are drawn at the original width, in the 4:3
middle. So the wide picture was squeezed into the middle and the sides had
nothing.

**Fix** (`rt64/0022`): a draw that samples a copy of the screen (orthographic,
or perspective like the page turn's curling page)
is stretched across the widened screen (RT64 already does this for
rectangles), so each point samples the copy where it is. 4-2: the reflection
spans the whole width and lines up with the platforms; Original aspect is
unchanged.

## Round 43: lines through strip images, wiggling edges (issues #16, #22)

**Report:** after Yoshi stops, the left edges of 1-1's tall hills wiggle for a
moment before settling; the castle screen's clouds wiggle at their left edge
as they drift; a line shows in message boxes as the text scrolls. At every
resolution and framerate; not in emulators.

**Test:** a temporary switch (`DINOTALE_SNAP`) placed sprites and background
layers on whole native pixels. Backgrounds: the hills' wiggle went away.
Sprites: the clouds' edges stopped wiggling but the lines through clouds and
message boxes got bigger.

**What was tried** (each tested on Windows at high resolution):

| Backgrounds | Sprites | Hill edge | Fence, fruit | Message boxes, castle clouds |
|---|---|---|---|---|
| sub-pixel | sub-pixel (0013) | wiggles | ok | tiny line; cloud edges wiggle |
| whole texels | start rounded, size kept | ok | fence wiggles | bigger lines |
| whole texels | exact sub-pixel quads | wiggles | fence much worse | - |
| whole texels | all on whole pixels, fitted | ok | fence much worse | perfect |
| sub-pixel | all on whole pixels, fitted | wiggles | fence much worse | perfect |
| whole texels | strip images fitted, others 0013 | wiggles | fence, fruit wiggle | perfect |

The hill edge is only calm when backgrounds and every sprite are on whole
pixels, which suggests sprites drawn over a background layer sliding against
it; but small sprites fitted to whole pixels change width by a pixel now and
then and wobble.

**Kept:** large images drawn in strips (G_OBJRM_SHRINKSIZE, at least 64
texels wide: message boxes, clouds) are drawn on whole pixels, both edges
rounded and the image fitted exactly between them (a quad, the path rotated
sprites use): neighbouring strips share their edge, so no lines, and the
image moves with its edges. Backgrounds and other sprites are as before.
The hill wiggle (#16) stays open.

| whole texels, rounded | all on whole pixels, rounded, width kept | mostly ok | fence less, fruit wiggles (more moving left) | perfect |
| whole texels, rounded down | all on whole pixels, rounded up (RDP), width kept | mostly ok | fence, fruit wiggle | perfect |

**Kept:** the fence fix version (large strip images on whole pixels with their
width kept; backgrounds and other sprites as before): fence and fruit calm,
message boxes and clouds perfect. The hill edge's wiggle stays (#16); with
everything on whole pixels it's mostly gone but sprites near Yoshi wiggle,
which is more noticeable. It may partly be the game's own layer separation.

The camera was ruled out: logged per frame in Original aspect it eases to a
stop smoothly, and the hill wiggles with or without the widened camera-wall
probe (temporary `DINOTALE_NARROW_CAMERA` switch, now removed). So back to
the switch's combination (whole-texel backgrounds, every sprite on whole
pixels), without its two faults: sprites keep their width (and, unless
they're strips, their height), and strips share their rounded edges.
Rounding both to the nearest pixel let a sprite and the scenery it stands in
land a pixel apart at half-pixel positions, depending on direction (worse
moving left). The N64 rounds a background's scroll down (point sampling) and
a rectangle's start up (the RDP starts on the first pixel at or after it):
for an object at a whole-pixel place in the world those agree. Both now
round the N64's way.

**Picket fence:** it's one 64x32 sprite at scale 1.003 with the strip flag
(render mode 0x18), so it took the strip path, where rounding both edges made
it 64 or 65 pixels wide by turns. Horizontally the width is now rounded once
(start rounded, size kept); vertically both edges are rounded, which is where
strips meet. Frame-by-frame captures at 3x: the pickets keep their shape.

## Round 42: lines through the castle screen's clouds (issue #22)

**Report:** on the continue / game over screen (the castle), the clouds have
a single line across them.

**Cause:** each cloud is a translucent 128x64 image drawn in strips (32, 32
and 2 rows) by G_OBJ_SPRITE with a scale. `rt64/0013` starts each strip's
rectangle on a whole pixel, rounding its start down, but the renderer rounds
a rectangle's far edge up (`FixedRect::right(true)`), so where a strip ended
on a fraction of a pixel the next one started on the same pixel and drew that
row again. In an opaque image that's invisible; in a translucent one the row
is blended twice, a line.

**Fix:** round each strip's start up instead, the way the renderer rounds the
previous strip's end, still moving the texture by the difference so the image
lands at its exact position. The castle screen's clouds: no lines.

## Round 41: things vanishing at the 4:3 edge in Expand (issues #19, #20)

**Reports:** in 5-4, Shy Guys on stilts have no stilts until they're inside
the 4:3 part of the screen, and the bob-ombs fired from the ship vanish at
the 4:3 edge (they vanish off screen on the N64 too, but at its edge).

**Cause:** the game decides what's on screen from each actor's screen x
(actor +0x34, in the 320-wide view's pixels). The shared test,
`func_800591EC(x, y, margin_x, margin_y)`, passes when -margin < x < 320 +
margin (the camera's width, +0x9C); the draw cull (`func_8005F700`, called
with margins from 0 up), the "is it on screen" helpers and others use it.
In Expand the extra strips are outside 0..320, so anything there with a
small margin wasn't drawn or was removed. The stilts are drawn by
`func_805175E4` (ovl_096) only while 0 <= x < 320.

**Fix:** `dinotale_actor_screen_x` gives such a test the x it would see if
the screen were the wider view: unchanged inside 0..319, the nearest edge
column inside the extra strips, moved in by the strips' width past them, so
bounds at or past the screen's edges follow the wider view's edges. Hooked
at the entry of `func_800591EC` and before the stilts' test. Checked that
the shared test runs every frame in 5-4.

## Round 40: Windows crashes in the NVIDIA driver (issues #6, #17)

**Reports:** random crashes in 4-1 and 4-4 (four in one session in 4-4),
jumping or standing still. Every crash log is the same: access violation at
offset 0x7482ee in `nvwgf2umx.dll` (NVIDIA's graphics driver), on the Gfx
Thread, writing an address 0x20 below a 1 MB-aligned one (`...d7fffe0`,
`...a29fffe0`).

**Cause (most likely):** the Gfx Thread running out of stack inside the
driver. MSVC gives every thread created without an explicit size (all
`std::thread`s: the Gfx Thread, RT64's workers, the game's threads) a 1 MB
stack, and the driver compiles shaders on the thread that asks for them;
NVIDIA's compiler is known to need more than that. A write just below a
1 MB boundary is where a 1 MB stack ends. Linux threads get 8 MB, and no
crash like this has been reported there. Not reproducible here.

**Fix:** link with `/STACK:8388608`: 8 MB stacks, as on Linux. Only reserved;
memory is committed as a stack actually grows.

## Round 39: smearing past 16:9 in Expand (issue #15)

**Report:** in widescreen the outer left and right edges of the screen
smear and flicker ("hall of mirrors") all the time. The screenshots are from
windows wider than 16:9 (2000x1055, 1365x720).

**Cause:** Expand widens the game's view up to 16:9, but RT64's Expand used
the window's full aspect ratio, so in a wider window the strips past 16:9
were part of the picture with nothing drawn in them: whatever an earlier
frame left there stayed (black here on lavapipe, which starts from cleared
memory).

**Fix** (`rt64/0020`): with the game's view widened (`extendScreen`), RT64's
target aspect ratio stops at 16:9, so a wider window shows the 16:9 picture
with black bars. Hybrid, which stretches backgrounds to the window, is
unchanged. Checked in a 1500x700 window.

## Round 38: lines through rotated sprites (issues #12, #13, #14)

**Report:** thin lines through the tilting platforms in 3-3, the balls in
4-3 and the cogwheels in 6-1: all sprites the game rotates (G_OBJ_SPRITE
with a rotation in its 2D matrix, drawn by `rt64/0014`). Flat sprites of the
same objects next to them are fine.

**Cause:** `rt64/0014` draws a rotated sprite through RT64's rectangle path,
over its bounding box, with the box's four corners moved to where the matrix
puts them, as positions from -1 to 1 across the box. The box was on quarter
pixels, but the renderer places a rectangle with its edges rounded up to
whole pixels (`FixedRect::left(true)` etc.), so each rotated sprite was
stretched and moved by up to a pixel (several screen pixels at high
resolutions), by a different amount for each piece. The game draws large
sprites in TMEM-sized strips, so the joins between a sprite's strips opened
into lines. The lines don't pass through the sprites' centres, so it isn't
the diagonal between a quad's two triangles.

**Fix:** the bounding box is on whole pixels, and the corners are placed
against the box as the renderer rounds it. Not reproducible here (lavapipe,
and the tested rotated sprite, 1-4's block, is a single strip); 1-4's block
still rotates correctly.

## Round 37: Yoshi jumping at the 4:3 edge in Expand (issue #8)

**Report:** in widescreen, walking towards the edge of the screen, Yoshi
"breaks at the 4:3 mark and appears instantly on the edge of the screen".

**Cause:** func_800233B4 keeps Yoshi on screen: when his screen x less/plus
half his width passes 16 or 304 (the 320-wide screen's edges less 16 px), it
calls func_80023310, which puts him back at the camera's centre -/+ 144.
Round 32 (issue #4) moved that spot out to the wider view's edge but not the
check, so crossing the 4:3 edge teleported him 56 px outwards. At a level's
start, where the wider view reaches past the left end of the level, that
dropped him off the level and he lost a life (reproduced at 1-1's start).

**Fix:** the checks (hooks in func_800233B4) and the spot move out together to
16 px inside the wider view, so he walks smoothly to the visible edge. On the
left the limit is never past world x 16, where the N64 stops him at a level's
start (the camera's 320-wide view starts there, the level art at x 0). 1-1:
walking left from the start, he now stops at the edge of the ground.

## Round 36: stale background blocks in 4-1 (issue #7)

**Report:** many graphical glitches in 4-1, in Original and widescreen:
rectangular blocks of the wrong background around the treehouse.

**Reproducing:** the selected page and course are bytes at `D_800F7EA0
+0xF5C` / `+0xF5D` (0-based; the code reads them as a pair). Holding the page
byte at 3 with gdb while course 1 starts loads 4-1 from a fresh save
(`page.sh` in the test harness).

**Cause:** the game draws newly exposed background tiles into its CI8 ring
buffers with the RDP, and reads the buffers as textures from RDRAM (rt64/0004,
since a GPU copy can't hold colour indices). RT64 only copies framebuffers
back to RDRAM with its render-to-RAM option, which is off, so those tiles
never reached RDRAM and whatever the buffer held before showed in their
place. Most levels write nearly all tiles with the CPU, so it rarely showed;
4-1's treehouse zooms the camera out to 70% (all layers drawn at scale
1461/1024, plus a 480x352 layer) and streams far more tiles through the RDP.

**Fix** (`rt64/0019`): framebuffer pairs with an 8-bit colour image are
always rendered back to RDRAM; everything else is unchanged. Same play
sequence compared before and after: the stray tiles (stone blocks, cut
strips of the hut) are gone.

**Expand:** the 480-wide layer is now widened in proportion like the
320-wide ones (to 656 for 16:9), and row requests grow by the same factor
(30 columns -> 41), so the zoomed-out treehouse fills the wider screen.

## Round 35: crash log detail (issue #6)

**Report:** a crash in level 4-1 (0.1.0, Windows): access violation writing
host address 0x2658d7fffe0, outside N64 memory, "not in a recompiled game
function", fault offset 0x7482ee; only func_807C6C00 on the stack.

**What it tells us:** func_807C6C00 is a fade object (counts a byte up by 8
to 255, then starts a scene change through func_8006E0FC/func_80068CF8);
it and its callees only store into RDRAM, so the faulting code is elsewhere.
A single game function in 64 KB of stack rules out a deep stack overflow
and suggests the fault may not even be on the game thread. The offset can't
be looked up: the log didn't say which module it was in (a driver DLL is
possible), and release builds had no symbols.

**Change:** the crash log now names the module the fault is in and the
faulting thread (GetThreadDescription / pthread_getname_np), and Windows
builds write `DinoTale.map` (linker map, build folder only, not shipped) so
an offset in DinoTale.exe can be looked up. Asked the reporter for what was
happening at the time and their settings.

## Round 34: Hybrid Dimming setting (issue #5)

**Report:** on an ultrawide (21:9) screen Hybrid draws the background across
the whole window but dims the sides; the reporter would like them undimmed.

**Change:** Settings > Graphics > Hybrid Dimming, a percent slider (default
88%, the 0.1.0 look; 0% leaves the sides undimmed). Disabled unless the
aspect ratio is Hybrid. `src/main/display_options.cpp` (was
`screen_filter.cpp`) hands it to RT64 every VI
(`RT64::setHybridSideBrightness`, `rt64/0018`).

Checked in a 21:9 window: Hybrid's background fills it edge to edge, and
the dimming covers both sides. Expand stays capped at 16:9 (wider needs
larger background buffers in the game's memory; not done).

## Round 33: screen filters (issue #1)

**Request:** texture filtering such as xBRZ and HQ2x/3x/4x.

**Change:** Settings > Graphics > Screen Filter: None, CRT, Scanlines, LCD,
xBR, xBRZ, HQ2x. All drawn in RT64's VI pass (`rt64/0017`), on the frame's
grid of N64 pixels so they look the same at any resolution setting, with the
picture still read at full resolution:

- CRT, Scanlines and LCD come from the Turok: Rage Wars port's patch (same
  RT64 setup): Lottes' public-domain CRT shader without the tube curve, a
  cosine scanline dip, LCD3x (public domain).
- xBR (Hyllian's xBR-lv2, MIT), xBRZ (freescale, MIT shader, GPL-3 xBRZ
  algorithm) and HQ2x (Lior Halphon's, MIT) are ported from libretro's
  glsl-shaders to HLSL. Their notices are in the shader and in
  `licenses/screen-filters.txt`, which the release packages include.
- In a widened picture (Expand, Hybrid) the N64 pixels stay square, so the
  grid is wider; Hybrid's dimmed sides still apply.
- HQ3x/HQ4x need lookup-table textures in libretro's versions, a second
  texture in the VI pass; not done yet.

`src/main/screen_filter.cpp` adds the option to the graphics tab and hands it
to RT64 every VI (`RT64::setScreenFilter`). `cmake/ApplyLibPatches.cmake` now
removes files a patch adds before applying the set again (as in the Turok
port), since resetting the submodule only restores tracked files.

## Round 32: Expand, invisible wall short of the screen edge (issue #4)

**Report:** in level 3-2 in widescreen, Yoshi can't enter the outer edge of
the screen, as if a barrier were there; fine in 4:3.

**Cause:** `func_80023310` keeps Yoshi on screen: past the camera centre
-/+ 144 (16 px inside a 320-wide view) it pushes him back. In Expand the
camera stays 320 wide (round 20) and its wall probe is widened, so where the
camera stops (camera walls, level ends) the wider view reaches 56 px past the
camera's own edge, and Yoshi was stopped 56 px before the visible edge.
Reproduced at Treasure Hunt's start after scrolling away and back; found by
watching Yoshi's x with gdb while he pushed against the wall.

**Fix:** hooks on the two `lui` of -/+144 load the wider view's half width
less 16 (200 for 16:9) in Expand (`dinotale_screen_margin_bits`). Other
modes keep 144.

Also: Hybrid and Expand are marked "(experimental)" in the aspect ratio
description (`RecompFrontend/0001`) and the README.

## Round 31: game picked from a .zip or .7z

**Request:** let players pick the game inside an archive (.zip, .rar, .7z).

**Change:** when the picked file isn't a ROM, librecomp's `select_rom` now
hands its contents to an archive reader the game registers
(`N64ModernRuntime/0003`), then checks the result exactly as before.
`src/main/rom_archive.cpp` reads `.zip` with miniz (MIT, already part of
N64ModernRuntime) and `.7z` with the LZMA SDK's 7z decoder (public domain,
`thirdparty/lzma-sdk`, the files the SDK's own 7z example builds). The first
file inside with an N64 ROM header, in any byte order, is used. `.rar` is
left out: the official decoder's license isn't GPL-compatible.

Tested: zip and 7z (LZMA, solid with another file, PPMd), a byte-swapped
`.n64` inside, an archive with no ROM (the usual "not a valid ROM" message),
and the launcher's file picker end to end (the stored copy matches the
`.z64`).

## Round 30: pre-release cleanup (0.1.0)

- **Privacy sweep:** every commit message, author and file in the published
  history checked for personal details, session links, local paths, keys
  and game data. Session links were removed from the history earlier; the
  sweep found nothing else. No ROM, screenshot or extracted game file was
  ever committed. `CLAUDE.md` records the rules for future commits.
- **Code:** unused includes and a leftover variable removed from
  `src/game/widescreen.cpp`; the health flower hook checks the actor before
  reading the settings (it runs for every actor drawn).
- **Docs:** README status and display options updated, a Playing section and
  `CHANGELOG.md` added, PROGRESS tidied. Version 0.1.0.
- **Packaging:** the release zip also carries `CHANGELOG.md`; SlotMap's
  license notice path fixed (it lives in `slot_map.h`).

## Round 29: message box frame stretched to the window for a frame

**Report:** when a message box opens, its wooden frame's top rail shoots out
to the edges of the screen for a split second before it settles.

**Cause:** the frame opens with a spring animation (the game scales its
pieces, drawn as `G_OBJ_SPRITE` with the 2D matrix, to about 1.2x and back).
At the peak the rails span x = -2 to 322, just past both edges of the
320-wide screen. RT64 treats a rectangle that spans the whole screen width as
a background and stretches it to the window in a wider aspect ratio, so the
rails jumped to the window edges for that frame.

**Fix** (`rt64/0016`): sprites drawn through the 2D matrix (`G_OBJ_SPRITE`,
`G_OBJ_LDTX_SPRITE`) keep their shape (`G_EX_ASPECT_ADJUST`) unless the game
chose an aspect itself. Plain sprite rectangles keep the old rule, so a
full-screen overlay drawn as one still fills the window.

## Round 28: Expand, stale tiles when scrolling vertically

**Report:** in Tower Climb, blocks of the wrong background tiles show up as
the screen scrolls up.

**Cause:** as the camera moves vertically, the game queues a row of new tiles
for each background layer (`func_8004FB98`). The row starts at the wider
view's left column, but its length comes from the level data: 20 columns,
plus one when the view isn't on a tile boundary. In Expand the view is 27-28
columns wide, so the last 7 columns of each new row were never drawn and kept
the tiles from further down the level (dark tiles over the blue section, and
so on). Rows were logged with a gdb breakpoint on the tile-run drawer
(`func_8005F2CC`): every row request was 21 columns.

**Fix:** a hook where the count is stored (0x8004FBF4) adds the view's extra
columns (`dinotale_bg_row_columns`). Other modes are unchanged.

## Round 27: Hybrid, dimmed sides

**Report:** in Hybrid, things pop in near the sides of the screen; only the
backgrounds are drawn there.

**Why:** Hybrid stretches the backgrounds of the original 4:3 view across
the window while sprites keep their shape, so the sprites stay in the middle
4:3 area. Objects appear at its edge, though the stretched scenery seems to
show their part of the level already. (Expand is the mode with a wider view.)

**Change** (`rt64/0015`): in Hybrid the final picture's sides outside the
4:3 area are dimmed (to 12%), so they fill the window without drawing the
eye. The HUD stays inside the 4:3 area in Hybrid (HUD Placement only spreads
it in Expand), so it's never dimmed. The Hybrid description in the menu says
so (`RecompFrontend/0001`).

## Round 26: rotated sprites

**Report:** in Tower Climb (Page 1, course 4) the flower platforms didn't
tilt when Yoshi stood near an edge, though Yoshi tilted.

**Cause:** RT64's S2DEX `G_OBJ_SPRITE` (and `G_OBJ_LDTX_SPRITE`) ignored the
2D matrix's rotation terms (B, C): a rotated sprite was drawn unrotated over
its bounding box. The N64 microcode draws such a sprite as two textured
triangles.

**Fix** (`rt64/0014`): a rotated sprite still goes through RT64's rectangle
path, over its bounding box, so it is placed and scaled in every aspect mode
like any other sprite. The rectangle's four corners are then moved to where
the matrix puts the sprite's corners, with the texture coordinates of those
corners (flips and SHRINKSIZE handled as for unrotated sprites). The RDP gets
a one-shot `quadOverride` for that.

## Round 25: HUD Placement setting with Expand

**Report:** Expand with HUD Placement set to Original still put the HUD at
the window edges.

**Fix:** the HUD hooks (round 23) now follow the existing HUD Placement
setting. **Original** keeps the fruit frame and health flower on the middle
4:3 part of the screen, while the background still uses the wider view.
**16:9** and **Expand** put them at the window edges; the view is at most
16:9 wide, so the two are the same.

## Round 24: HUD back to 4:3 outside Expand

**Report:** the widened HUD (round 23) showed in Original too.

**Cause:** the HUD hooks (and the camera's edge stop) used the running
level's view width, which is fixed when the level starts. A level started in
Expand kept the wide HUD after switching to another mode.

**Fix:** those hooks now use the level's width only while Expand is
selected, and the 4:3 width otherwise, so switching mid-level puts the HUD
back on the 4:3 screen at once. The level's wider background stays as it was
set up. Also a safeguard for the layer count that fixes the width for each
level: if a new layer is created after none were created or drawn for 3
seconds, the previous level has ended, so counting starts again. Then a
level left some other way can't carry its width into the next one.

## Round 23: Expand, HUD at the window edges

Yoshi's Story's in-level HUD is the fruit frame (30 slots around the edge of
the screen that fill with the fruit Yoshi eats) and the health flower in the
top left. In Expand both now use the wider screen:

- **Fruit frame:** `func_80579F18` (ovl_128) reads the 30 slot positions
  from the frame object (x at `+0x48`, y at `+0xC0`, floats relative to the
  middle of the screen) and writes the slot sprites. A hook at its start
  spreads the x positions by the width ratio (432/320), and one at its end
  restores them, so the frame keeps its shape around the window.
- **Health flower:** two actors (ids `0x4010`/`0x4011`) that keep themselves
  60 px right of the camera's left edge. A hook at the end of
  `func_8004733C` (actor to screen position) moves just those two left by the
  extra margin.

Found by logging the S2DEX object positions in RT64, then watching the
sprite and matrix structs for the game code that writes them. Original,
Stretch and Hybrid are unchanged.

## Round 22: text box shimmer, part 2

**Report:** text scrolling in message boxes still shimmers slightly.

**Cause:** each strip of the text image (round 12) was placed by rounding its
start to a quarter pixel on its own, with its texture always starting at the
first texel. Neighbouring strips of the same image then disagreed by up to a
fraction of a pixel, by an amount that changed every frame as the text
scrolled and the strips' sub-pixel positions changed.

**Fix** (`rt64/0013`): with clamped texture coordinates (the usual case,
text boxes included), an object rectangle now starts on a whole pixel and its
texture is moved back by the exact amount cut off, so every strip maps the
image to the same screen positions. Title, menus, course select, pause panel
and level sprites checked for regressions. Not yet confirmed on the text
boxes themselves (the test setup couldn't reliably reach a message block).

## Round 21: Expand, black edge at a level's start

**Report:** walking back to a level's start in Expand, the strip past the
level's left end filled with junk again (the start itself was black).

**Cause:** each layer's view reaches left of the layer's art there. The game
streams no art for those columns, so they keep whatever the ring buffer held
from further along the level. Clearing those ring columns from the CPU, or
with an RDP fill in the game's display list, reached RDRAM but not the
screen, because RT64 keeps its own copy of the ring.

**Fix:** before each layer is drawn, a hook (`func_8005F5D0` / `func_8005F690`,
before `func_8007DB34`) works out how many of its columns lie left of the art
(`-left` from the view object's `+0x18`) and passes that to our RT64 patch by
the layer's image address. When RT64 draws that background it fills the
strip with black (`rt64/0012`, `BgLeftMask`). Layers draw back to front and
the far ones reach further left, so each covers only its own junk. The level
start and walking back both show a clean black edge, the storybook look.
Confirmed on Windows. Sprites (enemies, items) can still be drawn over the
black strip; that's the game's own content, not garbage, and left as is
since players rarely walk back to that edge.

## Round 20: Expand, objects shifted (round 18 reverted)

**Report:** in Expand everything the game positions itself was shifted right
by the width it gained on each side, in levels and in menus such as course
select.

**Cause:** round 18 widened the camera object's own width (+0x9C). The
camera's position (+0x0) is its left edge, and many places in the game and its
overlays (around 40 files read the camera) measure from it: the pause panel,
menus, objects. With a wider camera that edge moved left by 56, and only the
two places that add 160 to get the centre had been adjusted. Round 18 also
applied the wider camera in menus.

**Now:**

- The camera keeps its 320 width, so everything the game places stays where
  it is on the N64.
- The horizontal camera-wall probe stays widened (`func_8002A148`), so a
  moving camera stops before the wider view crosses a level's end.
- At a level's start the camera is placed for a 320-wide view, so the wider
  view reaches past the level's art. Each background's ring buffer is now
  cleared when it's created (hook after `func_8004EEEC`), so that strip is
  black instead of whatever an earlier level left in that memory.
- Tried and dropped: pushing the camera over at the level start (velocity or
  direct). It aligns correctly, but the background streaming leaves stale
  columns, mostly in the slow far layers; asking the layers for a full redraw
  (`+0x1C0 = func_8005F5D0`, view `+0x0`) didn't cure it.

## Round 19: Expand can only be chosen outside a level

Expand takes effect when a level starts, since that's when the game creates
its background layers and camera, and switching to it in the middle of one
glitched. Now:

- `src/game/widescreen.cpp` counts the live background layers (created in
  `func_8004F2B0`, freed in `func_8004F38C`, both hooked). The view width
  is worked out when a level's first layer is created and kept until its
  last is freed, so all the hooks agree for the whole level, even if the
  setting changes meanwhile.
- While a level that started without Expand is running, the Expand choice
  is greyed out in Settings > Graphics > Aspect Ratio, and the description
  says it can only be turned on outside a level. Inside a level started with
  Expand, switching away and back is allowed. (`RecompFrontend/0001`: a
  thread-safe `set_expand_available`, applied by the options page on the UI
  thread.)

## Round 18: Expand, camera covers the wider view

**Problem:** at a level's left end, Expand showed background-buffer columns
the game never filled, because the camera stopped where a 320-wide view
would.

**Findings** (`frameManager`, `bgManager`, `1E360`):

- The camera object (pointer at `D_800F7EA0+0x478`) keeps its own view size,
  set to 320 x 240 when it's created (`func_8005A9BC`). Its left/top corner
  is at `+0x0`, its centre at `+0x30`, its corners after that. The follow
  logic and the clamp at the level's ends work from that width.
- The camera is stopped by camera-wall tiles in the level data, probed at
  centre +/- 160 (`func_8002A148`, horizontal movement).
- Two places turn the camera's left edge into its centre by adding 160: the
  background view's centre (`func_8004FEC8`) and the world-to-screen
  conversion for objects (`func_80058CC0`).
- `func_800591EC`, the on-screen test for objects, uses the camera width.

**Fix** (hooks in `yoshisstory.us.toml`, values from
`src/game/widescreen.cpp`), in Expand only: the camera is created with the
widened width (432 for 16:9), and the wall probe and both centre offsets use
half of it (216). The level start now shows real scenery to the window's
left edge, and the backgrounds, sprites and Yoshi stay aligned. Original,
Stretch and Hybrid keep 320/160.

Still to check in Expand: objects at the new edges (spawning, culling), the
vertical wall probe (`func_8002A630`, left at 160: widening it pushed the
camera off at the level start), and the HUD.

## Round 17: top specks and right-edge strip

- **Top specks (Expand/Hybrid, higher resolutions):** the first row the game
  draws samples, with bilinear filtering, the wrapped last row of the sky
  layer's ring buffer, which the game never fills. At 1x it barely
  contributes; upscaled, the first few output rows take most of it. The
  output mask now covers the top two rows (`rt64/0011`).
- **Right-edge strip (Expand, higher resolutions):** the misalignment
  correction also rounds the window-anchored right scissor edge down, leaving
  a few pixels undrawn. It's pinned to the window edge (`rt64/0011`).

## Round 16: Expand at higher resolutions

Expand drew nothing but 3D models (Yoshi) on the test setup at 2x/3x
resolution: every S2DEX background and sprite was missing. RT64's
"misalignment" correction for upscaled rendering moved the widened scissor's
left edge to x = -1, and a negative scissor offset is invalid in Vulkan.
Some drivers skip the draw, others clamp it, which is why it worked on the
reporter's machine. Fix (`rt64/0010`): converted scissors are clamped to 0.

## Round 15: Expand, mirrored strip on the right

**Report:** in Expand the right-hand strip of the far background looked
mirrored; hills scrolled in from the wrong direction.

**Cause:** painting marker stripes into the background buffer showed the
strip was a true mirror image around the old 4:3 right edge. RT64 works out
how much of a texture a rectangle samples, clipped to the scissor, and
uploads only that much as a GPU texture. The widened scissor still stored
its 4:3 coordinates (the extension comes from its extended-GBI origin), so
the texture was cut off at the 4:3 edge, and the tile's mirror addressing
reflected it from there.

**Fix** (`rt64/0009`): a scissor edge with an extended origin no longer
limits the sampling range.

**Still open:** at a level's left end, Expand shows unfilled buffer (the
camera limit is the 4:3 one). The camera code is in `frameManager`
(0x80058CC0-0x8005AA68), which also holds the 4:3 constants
(`D_800B0B90`-`D_800B0BA0`, 1.3333) used by the on-screen checks for
objects: those will widen sprite/enemy culling.

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
