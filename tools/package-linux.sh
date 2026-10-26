#!/usr/bin/env bash
# Builds the Release game on Linux and packages it as a .tar.gz for
# distribution (Steam Deck and other x86-64 Linux PCs).
#
# Run from the repo root, after the one-time steps in BUILDING.md (N64Recomp
# run, so RecompiledFuncs/ exists, and the ROM at yoshisstory.us.z64):
#
#   tools/package-linux.sh 0.1.7
#
# Builds with the linux-release preset (or an already configured build
# folder: BUILD_DIR=build tools/package-linux.sh 0.1.7), then stages the
# executable, the assets folder, README, CHANGELOG, license and the
# third-party license notices into dist/DinoTale-<version>-linux.tar.gz.
# The executable uses the system's SDL2, GTK 3 and FreeType (all present on
# SteamOS) and links libstdc++ statically.
set -euo pipefail

version="${1:-0.1.7}"
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [ -z "${BUILD_DIR:-}" ]; then
    cmake --preset linux-release
    cmake --build --preset linux-release
    BUILD_DIR=build/linux-release
else
    cmake --build "$BUILD_DIR" --target DinoTale
fi

name="DinoTale-$version-linux"
stage="dist/$name"
rm -rf "$stage"
mkdir -p "$stage/licenses"

cp "$BUILD_DIR/DinoTale" "$stage/"
strip "$stage/DinoTale"
cp -r assets "$stage/"
cp README.md CHANGELOG.md COPYING "$stage/"

# License notices for the third-party code built into the executable.
while IFS='=' read -r lib path; do
    if [ -e "$path" ]; then
        cp "$path" "$stage/licenses/$lib.txt"
    else
        echo "warning: license file not found: $path" >&2
    fi
done <<'EOF'
N64ModernRuntime=lib/N64ModernRuntime/COPYING
N64Recomp=lib/N64ModernRuntime/N64Recomp/LICENSE
RT64=lib/rt64/LICENSE
plume=lib/rt64/src/contrib/plume/LICENSE
hlslpp=lib/rt64/src/contrib/hlslpp/LICENSE
imgui=lib/rt64/src/contrib/imgui/LICENSE.txt
implot=lib/rt64/src/contrib/implot/LICENSE
im3d=lib/rt64/src/contrib/im3d/LICENSE
ddspp=lib/rt64/src/contrib/ddspp/LICENSE
nativefiledialog-extended=lib/rt64/src/contrib/nativefiledialog-extended/LICENSE
stb=lib/rt64/src/contrib/stb/LICENSE
xxHash=lib/rt64/src/contrib/xxHash/LICENSE
zstd=lib/rt64/src/contrib/zstd/LICENSE
re-spirv=lib/rt64/src/contrib/re-spirv/LICENSE
miniz=lib/N64ModernRuntime/thirdparty/miniz/LICENSE
o1heap=lib/N64ModernRuntime/thirdparty/o1heap/LICENSE
RmlUi=lib/RecompFrontend/recompui/lib/RmlUi/LICENSE.txt
lunasvg=lib/RecompFrontend/recompui/lib/lunasvg/LICENSE
plutovg=lib/RecompFrontend/recompui/lib/lunasvg/plutovg/LICENSE
GamepadMotionHelpers=lib/RecompFrontend/lib/GamepadMotionHelpers/LICENSE
SlotMap=lib/RecompFrontend/lib/SlotMap/slot_map.h
LZMA-SDK=thirdparty/lzma-sdk/lzma-sdk.txt
screen-filters=licenses/screen-filters.txt
EOF

tar -C dist -czf "dist/$name.tar.gz" "$name"
echo "Packaged dist/$name.tar.gz"
