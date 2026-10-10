#!/usr/bin/env bash
# Regenerate YoshiStorySyms/yoshisstory.us.syms.toml from your own ROM.
#
# Only needed when changing how symbols are derived; the generated file is
# committed. Needs: git, make, python3 (+venv), binutils-mips-linux-gnu, wget.
#
# Steps:
#  1. clone the Yoshi's Story decomp at a pinned commit and build it (this
#     also checks that your ROM is the right one: the build must match)
#  2. run splat over the decomp's yamls plus our overlay segments
#     (tools/gen_overlay_yaml.py)
#  3. combine splat's function list, the decomp ELF's libultra names and the
#     ROM's own overlay relocation tables (tools/make_syms.py)
#
# Usage: tools/regen_symbols.sh path/to/yoshisstory.us.z64 [workdir]
set -euo pipefail

DECOMP_URL=https://github.com/decompals/yoshis-story
DECOMP_COMMIT=82e891eea6c523684e1e1a43f0229a243858cf84

ROM=$(realpath "$1")
WORK=$(realpath -m "${2:-build-syms}")
REPO=$(cd "$(dirname "$0")/.." && pwd)

mkdir -p "$WORK"
if [ ! -d "$WORK/decomp" ]; then
    git clone "$DECOMP_URL" "$WORK/decomp"
fi
cd "$WORK/decomp"
git checkout -q "$DECOMP_COMMIT"
cp "$ROM" baseroms/us/baserom.z64
make venv
make setup
make extract
make lib
make -j"$(nproc)"

# Second splat run with the overlays split out too.
python3 "$REPO/tools/gen_overlay_yaml.py" baseroms/us/baserom.z64 > "$WORK/overlays.yaml"
cat yamls/us/header.yaml yamls/us/makerom.yaml yamls/us/main.yaml "$WORK/overlays.yaml" > full_with_overlays.yaml
rm -rf asm/us .splat
.venv/bin/python -m splat split full_with_overlays.yaml

mips-linux-gnu-nm -S --defined-only build/yoshisstory-us.elf > "$WORK/elf_nm.txt"
python3 "$REPO/tools/make_syms.py" baseroms/us/baserom.z64 .splat/spim_context.csv "$WORK/elf_nm.txt" \
    > "$REPO/YoshiStorySyms/yoshisstory.us.syms.toml"
echo "wrote YoshiStorySyms/yoshisstory.us.syms.toml"
