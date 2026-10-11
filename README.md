# DinoTale: Yoshi's Story Recompiled (work in progress)

A native PC port of Yoshi's Story (N64, 1997/98), made by statically
recompiling the game with [N64Recomp](https://github.com/N64Recomp/N64Recomp)
and running it on [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime)
with [RT64](https://github.com/rt64/rt64) for rendering. Windows first.

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own dump of the game to build or play this; the
game asks you for it on first launch.

**Status: playable, still being tested.** The game boots, plays and saves;
levels are being played through in every display mode to find what's left.
Progress is tracked in the open: [PROGRESS.md](PROGRESS.md) is the short
version, [STATUS.md](STATUS.md) the full working log.

## Display options

Settings > Graphics > Aspect Ratio:

- **Original:** the game's 4:3 picture, with black bars at the sides.
- **Stretch:** the 4:3 picture stretched to fill the window.
- **Hybrid:** backgrounds stretched to the window, sprites kept in shape in
  the middle 4:3 area; the stretched sides are dimmed.
- **Expand:** true widescreen (up to 16:9). The game shows more of each
  level. Choose it before starting a level. HUD Placement sets whether the
  HUD moves out to the window edges.

## Game files

Yoshi's Story (USA) (En,Ja). Any byte order (`.z64`, `.n64`, `.v64`) works.
SHA-1 of the big-endian `.z64`:
`B13072FEF6C6DF48C07D8822C01E5BC59036F6DA`
(`tools/normalize_rom.py` converts other byte orders and prints the header.)

## Playing

Download the latest Windows build from
[Releases](https://github.com/wootbeer/dinotale/releases), unzip it anywhere
and run `DinoTale.exe`. On first launch, pick your copy of the game; it's
checked against the hash above and never modified. Settings (graphics,
controls, sound) are in the in-game menu.

What changed in each version: [CHANGELOG.md](CHANGELOG.md).

## Building

See [BUILDING.md](BUILDING.md).

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
