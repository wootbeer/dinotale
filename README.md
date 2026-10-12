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

### Game files

Yoshi's Story (USA) (En,Ja). Any byte order (`.z64`, `.n64`, `.v64`) works,
and it can be picked inside a `.zip` or `.7z` (extract a `.rar` first).
SHA-1 of the `.z64`: `B13072FEF6C6DF48C07D8822C01E5BC59036F6DA`

## Display options

Settings > Graphics > Aspect Ratio:

- **Original:** the game's 4:3 picture, with black bars at the sides.
- **Stretch:** the 4:3 picture stretched to fill the window.
- **Hybrid** (experimental): backgrounds stretched to the window, sprites kept in shape in
  the middle 4:3 area; the stretched sides are dimmed.
- **Expand** (experimental): true widescreen. The game shows more of each level. Choose it
  before starting a level. HUD Placement sets whether the HUD moves out to
  the window edges.

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
