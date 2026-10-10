# patches/

Headers that RecompFrontend's recompui compiles against: its
`ui_api_events.cpp` includes `patches/ui_funcs.h` from the game project by
relative path, for `recomp_run_ui_callbacks` and the UI event structs.

There are no whole-function MIPS patches yet. Changes to the game are
instruction patches and hooks in `yoshisstory.us.toml`, with any native code
in `src/game/`, so the build needs no MIPS compiler.
