# Building Guide

The project is in early development; there is no playable build yet. This
covers what exists so far and will grow with it.

You need your own copy of the game: Yoshi's Story, USA (see README.md for
the hash). The ROM is never committed to this repository; the recompiled
code is generated from it locally.

## 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/wootbeer/dinotale.git
# if you forgot --recurse-submodules:
git submodule update --init --recursive
```

## 2. Put the ROM in place

Place your dump at the repo root as `yoshisstory.us.z64`. It must be
big-endian `.z64`; to convert:

```bash
python3 tools/normalize_rom.py "your dump.n64" yoshisstory.us.z64
```

## 3. Generate the recompiled code

```bash
cmake -S lib/N64ModernRuntime/N64Recomp -B lib/N64ModernRuntime/N64Recomp/build -G Ninja
cmake --build lib/N64ModernRuntime/N64Recomp/build --target N64RecompCLI
./lib/N64ModernRuntime/N64Recomp/build/N64Recomp yoshisstory.us.toml
```

This writes `RecompiledFuncs/` (gitignored).

## 4. Build and run

Dependencies on Ubuntu/Debian:

```bash
sudo apt-get install cmake ninja-build libsdl2-dev libgtk-3-dev libvulkan-dev lld llvm clang
```

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target DinoTale
./build/DinoTale
```

Run it from the repo root (it loads `assets/` from the working directory).
The audio microcode (`rsp/aspMain.cpp`) is generated from the ROM by the
build itself. Windows steps (Visual Studio 2022 with clang-cl) will follow
the BattleTanx port's and be written up here once tested.

## Regenerating the symbol file (optional)

`YoshiStorySyms/yoshisstory.us.syms.toml` is committed. To rebuild it from
your ROM (Linux, needs `binutils-mips-linux-gnu`, `python3-venv`, `wget`):

```bash
tools/regen_symbols.sh yoshisstory.us.z64
```
