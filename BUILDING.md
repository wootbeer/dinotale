# Building Guide

## Quick start: Windows with Visual Studio 2022

Needs Visual Studio 2022 with the "Desktop development with C++" workload
and its "C++ Clang Compiler for Windows" and "C++ CMake tools for Windows"
components, plus Git. Then, in PowerShell:

```powershell
git clone --recurse-submodules https://github.com/wootbeer/dinotale.git C:\dev\dinotale
cd C:\dev\dinotale
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\tools\setup-windows.ps1 -Rom "C:\path\to\your Yoshi's Story dump.n64"
```

The script converts and checks your ROM, generates the recompiled code and
builds `build\windows-release\DinoTale.exe`. Then open the folder in Visual
Studio (File > Open > Folder), pick the **Windows Release** configuration
(or **Windows Debug** for a build with debug info), choose **DinoTale.exe**
as the startup item and press F5.

After pulling changes, run `.\tools\setup-windows.ps1` again (no `-Rom`
needed) to regenerate the recompiled code, or just rebuild in Visual Studio
if only `src\` changed. `-Icon <png>` replaces the app icon.

If DinoTale.exe doesn't appear in the startup item list, Visual Studio's
own CMake configure failed: check View > Output > "CMake", then try
Project > Delete Cache and Reconfigure.

The rest of this guide is the same process step by step.

You need your own copy of the game: Yoshi's Story, USA (see README.md for
the hash). The ROM is never committed to this repository; the recompiled
code is generated from it locally.

## 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/wootbeer/dinotale.git
# if you forgot --recurse-submodules:
git submodule update --init --recursive
```

`main` has the latest work.

## 2. Install dependencies

### Windows

- Visual Studio 2022 with the "Desktop development with C++" workload,
  including the "C++ Clang Compiler for Windows" and "C++ CMake tools for
  Windows" components.
- Python 3 (only for `tools/normalize_rom.py`, if your dump isn't `.z64`).

That's all: the renderer uses D3D12 on Windows and CMake fetches SDL2
itself. (Unlike the BattleTanx port, there's no MIPS patch library, so WSL
isn't needed.)

### Linux (Ubuntu/Debian)

```bash
sudo apt-get install cmake ninja-build libsdl2-dev libgtk-3-dev libvulkan-dev lld llvm clang
```

## 3. Put the ROM in place

Place your dump at the repo root as `yoshisstory.us.z64`. It must be
big-endian `.z64`; to convert a `.n64`/`.v64` dump:

```bash
python3 tools/normalize_rom.py "your dump.n64" yoshisstory.us.z64
```

The game also asks for the ROM on first launch (any byte order) and checks
its hash.

## 4. Generate the recompiled code

Build N64Recomp and run it on the config. This writes `RecompiledFuncs/`
(gitignored; regenerate it after any change to `yoshisstory.us.toml` or the
symbol file).

**Windows** (in an "x64 Native Tools Command Prompt for VS 2022" or a
Developer PowerShell set to x64, see Troubleshooting):

```powershell
$env:DT_CLANGCL = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe"
cmake -S lib\N64ModernRuntime\N64Recomp -B lib\N64ModernRuntime\N64Recomp\build -G Ninja -DCMAKE_C_COMPILER="$env:DT_CLANGCL" -DCMAKE_CXX_COMPILER="$env:DT_CLANGCL"
cmake --build lib\N64ModernRuntime\N64Recomp\build --target N64RecompCLI
lib\N64ModernRuntime\N64Recomp\build\N64Recomp.exe yoshisstory.us.toml
```

**Linux:**

```bash
cmake -S lib/N64ModernRuntime/N64Recomp -B lib/N64ModernRuntime/N64Recomp/build -G Ninja
cmake --build lib/N64ModernRuntime/N64Recomp/build --target N64RecompCLI -j$(nproc)
./lib/N64ModernRuntime/N64Recomp/build/N64Recomp yoshisstory.us.toml
```

The main build (next step) generates the audio microcode (`rsp/`) itself.

## 5. Build and run

**Windows:**

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$env:DT_CLANGCL" -DCMAKE_CXX_COMPILER="$env:DT_CLANGCL"
cmake --build build --target DinoTale
.\build\DinoTale.exe
```

**Linux:**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target DinoTale -j$(nproc)
./build/DinoTale
```

Run it from the repo root: it loads `assets/` relative to the working
directory. `--game yoshisstory` skips the launcher once the ROM has been
selected. Configuring applies the patches in `lib-patches/` to the
submodules (they show as modified submodules afterwards). When a pull adds or
changes a patch, the next configure resets those submodules' files and
applies the whole set again, so local edits inside `lib/` are lost.

## 6. Package a release

Windows (PowerShell, after `tools\setup-windows.ps1 -Rom ...` has run once):

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\tools\package-windows.ps1 -Version 0.1.0
```

Builds through `tools\setup-windows.ps1` (the same Release build you play)
and writes `dist\DinoTale-<version>-windows.zip`.

Linux (after steps 1-4):

```bash
tools/package-linux.sh 0.1.0
```

Writes `dist/DinoTale-<version>-linux.tar.gz`.

Both contain the program, `assets/`, README, CHANGELOG, the license and the
third-party license notices. Never the ROM: players pick their own copy on
first launch. Test the packaged build before publishing it.

## Regenerating the symbol file (optional)

`YoshiStorySyms/yoshisstory.us.syms.toml` is committed. To rebuild it from
your ROM (Linux, needs `binutils-mips-linux-gnu`, `python3-venv`, `wget`):

```bash
tools/regen_symbols.sh yoshisstory.us.z64
```

## Troubleshooting (Windows)

**x64 shell.** The build must use the x64 toolchain. A Developer PowerShell
can default to x86, which fails late with `lld-link: undefined symbol:
mainCRTStartup`. `$env:LIB` must contain `\x64` paths. If not, open "x64
Native Tools Command Prompt for VS 2022" (in cmd, use `set
"DT_CLANGCL=..."` and `%DT_CLANGCL%`). `tools\package-windows.ps1` sets this
up itself.

**Which clang-cl.** Visual Studio ships a 32-bit-hosted `clang-cl.exe` in
`VC\Tools\Llvm\bin\` and a 64-bit one in `VC\Tools\Llvm\x64\bin\`. Pass the
x64 one by full path as above. Adjust `Community` to your edition.

**Starting over.** After changing compiler or architecture, delete the
build folder and configure again.
