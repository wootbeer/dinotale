# DinoTale: Yoshi's Story Recompiled

Yoshi's Story (N64, 1997/98), running natively on PC. The game is statically
recompiled with [N64Recomp](https://github.com/N64Recomp/N64Recomp), runs on
[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) and is
rendered with [RT64](https://github.com/rt64/rt64). For Windows and Linux,
including the Steam Deck.

Unofficial, and not affiliated with the rights holders. **No game files are
included:** you need your own copy of the game.

## Features

- Runs natively on Windows and Linux.
- **Widescreen:** play in 4:3, stretched, or true widescreen (Expand) that
  shows more of each level, up to 16:9.
- HUD at the edges of the window in widescreen, or kept in its original place.
- Screen filters: CRT, scanlines, LCD, and the xBR, xBRZ and HQ2x upscalers.
- Keyboard and controller support with remappable controls.
- Saves and sound.
- Cheats and GameShark codes, in the settings.

## Download and play

Get the latest build from
[Releases](https://github.com/wootbeer/dinotale/releases):

- **Windows:** unzip it anywhere and run `DinoTale.exe`.
- **Linux / Steam Deck:** extract the `.tar.gz` and run `DinoTale`. On the
  Deck, run it from Desktop Mode, or add it to Steam as a non-Steam game to
  play in Game Mode.

On first launch, pick your copy of the game. It's checked against the hash
below and never modified. Settings (graphics, controls, sound) are in the
in-game menu.

### Portable mode

Settings, saves and mods are kept in the game's own folder (`portable.txt`
next to `DinoTale.exe` or `DinoTale` marks it), so the folder can go
anywhere, a USB stick included. An install from before 0.1.9, which keeps
them in `%LOCALAPPDATA%\dinotale` (Windows) or `~/.config/dinotale`
(Linux), keeps using that: add a blank `portable.txt` to switch (and copy
them over), or delete it to go back. If the game's folder can't be written
to, the usual place is used.

### Command line

Most settings can be given on the command line, for a shortcut or a Steam
launch option. They're used for that run, unless `--save-settings` is given
too.

```
DinoTale --rom "Yoshi's Story.z64" --play --aspect expand --fullscreen
```

- `--rom <file>`: pick the ROM (also inside a `.zip` or `.7z`).
- `--play`: skip the launcher and start the game.
- `--fullscreen`, `--windowed`, `--resolution`, `--aspect`, `--hud`,
  `--framerate`, `--fps`, `--msaa`, `--downsampling`, `--api`,
  `--screen-filter`, `--texture-filtering`, `--hybrid-dimming`,
  `--frame-generation`, `--lsfg-multiplier`, `--lsfg-mode`, `--volume`,
  `--rumble`, `--deadzone`, `--background-input`: the settings of those
  names, for example `--screen-filter crt` or `--volume 50`.
- `--set <option>=<value>`: any setting, by its name in the settings files.
- `--list-settings` lists every setting with its value and choices; `--help`
  lists the options.

### Game files

Yoshi's Story (USA) (En,Ja). Any byte order (`.z64`, `.n64`, `.v64`) works,
and it can be picked inside a `.zip` or `.7z` (extract a `.rar` first).
SHA-1 of the `.z64`: `B13072FEF6C6DF48C07D8822C01E5BC59036F6DA`

## Display options

Settings > Graphics > Aspect Ratio:

- **Original:** the game's 4:3 picture, with black bars at the sides.
- **Stretch:** the 4:3 picture stretched to fill the window.
- **Hybrid** (experimental): backgrounds stretched to the window, sprites kept in shape in
  the middle 4:3 area; the stretched sides are dimmed (Hybrid Dimming sets
  how much, down to not at all).
- **Expand** (experimental): true widescreen. The game shows more of each level. Choose it
  before starting a level. HUD Placement sets whether the HUD moves out to
  the window edges.

### Lossless Scaling frame generation

Settings > Graphics > Frame Generation can use
[Lossless Scaling](https://store.steampowered.com/app/993090/Lossless_Scaling/)'s
frame generation (LSFG 3.1) instead of RT64's own, built in, with no
separate app running. It needs your own copy of Lossless Scaling's
`Lossless.dll`, from its install folder: press **Load Lossless.dll** and pick
it (it's copied into DinoTale's folder; nothing from Lossless Scaling comes
with DinoTale). Then set Frame Generation to **Lossless Scaling** and restart:
it runs on the Vulkan graphics API, which it switches to. Multiplier sets 2x,
3x or 4x the game's frame rate; Performance mode is lighter on the GPU. It
adds about one game frame of input delay. Windows and Linux.

## Cheats and GameShark codes

**Settings > Cheats** has the game's secrets without earning them, and a few
helpers:

- **Black and White Yoshi:** both are always there to pick (normally you find
  their eggs in 2-1 and 3-2). They join on the next Select Yoshi screen.
- **Yoshi Colour:** play as any of the eight Yoshis, in any mode, Practice
  included.
- **Infinite Health**, **Choose Any Course** (any course on each Story page),
  **Open All Trial Courses** (saved with your progress, so they stay open).

**Settings > Hacks** has GameShark codes from the usual code lists to switch
on, and **Custom Codes** for your own: type them as written, for example
`800F895F 0063; 810F8994 0100`. Codes are applied every frame, as a GameShark
does; 88 and 89 codes are applied when you press **GameShark Button**. Codes
that change the game's own code (most "infinite eggs" or "walk through
enemies" codes) can't work in a recompiled game and are skipped.

The pause code "hold L, then A, B, A, C-Up, Left" for invincibility and
"collect every coin for Purple Yoshi" from some cheat lists aren't in the
game: nothing in it checks for that button sequence, and Purple Yoshi was cut
before release. Infinite Health covers the first.

## Reporting issues

Found something wrong? Open an
[issue](https://github.com/wootbeer/dinotale/issues) with a screenshot, the
level and roughly where it happened, and which aspect ratio you were using.

## Building from source

See [BUILDING.md](BUILDING.md). What changed in each version is in
[CHANGELOG.md](CHANGELOG.md); [STATUS.md](STATUS.md) is the full development
log.

## Credits

- [N64Recomp and N64ModernRuntime](https://github.com/N64Recomp) by Mr-Wiseguy
  and contributors, which this port is built with
- [RT64](https://github.com/rt64/rt64) by Dario and contributors, the renderer
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by the N64Recomp
  contributors, the launcher and input layer
- The [Yoshi's Story decompilation](https://github.com/decompals/yoshis-story)
  by decompals, whose ROM layout, symbol names and overlay loader research
  this project builds on
- [splat](https://github.com/ethteck/splat), `spimdisasm` and `rabbitizer`,
  used to find the game's functions

## License

The project's own code is GPL-3.0; see [COPYING](COPYING). The game itself
remains the property of its rights holders.
