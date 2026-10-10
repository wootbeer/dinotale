#!/usr/bin/env python3
"""
Normalize a Yoshi's Story N64 ROM dump to big-endian .z64,
and print its header fields as a sanity check against a known-good USA dump.

Usage:
    python3 tools/normalize_rom.py "Yoshi's Story (USA) (En,Ja).n64" yoshisstory.us.z64

N64 ROM dumps circulate in three byte orders depending on the dumping
hardware/software used:
    z64  - big-endian, native order (what splat/N64Recomp/emulators want)
    v64  - byte-swapped within each 16-bit halfword
    n64  - word-swapped (effectively little-endian 32-bit words)
The file extension is not a reliable indicator of which one you actually
have ("*.n64" files are often byte-swapped v64 data) --
always detect from the magic bytes at offset 0x00.
"""
import hashlib
import struct
import sys

Z64_MAGIC = bytes.fromhex("80371240")
V64_MAGIC = bytes.fromhex("37804012")
N64_MAGIC = bytes.fromhex("40123780")


def normalize(data: bytes) -> tuple[bytearray, str]:
    magic = data[0:4]
    out = bytearray(data)
    if magic == Z64_MAGIC:
        return out, "z64 (already big-endian)"
    if magic == V64_MAGIC:
        for i in range(0, len(out), 2):
            out[i], out[i + 1] = out[i + 1], out[i]
        return out, "v64 (byte-swapped) -> normalized"
    if magic == N64_MAGIC:
        for i in range(0, len(out), 4):
            out[i:i + 4] = out[i:i + 4][::-1]
        return out, "n64 (word-swapped) -> normalized"
    raise ValueError(f"Unrecognized ROM magic: {magic.hex()}")


def print_header(data: bytes) -> None:
    clock = struct.unpack(">I", data[4:8])[0]
    entry = struct.unpack(">I", data[8:12])[0]
    release = struct.unpack(">I", data[12:16])[0]
    crc1 = struct.unpack(">I", data[16:20])[0]
    crc2 = struct.unpack(">I", data[20:24])[0]
    name = data[0x20:0x34].decode("ascii", errors="replace").strip("\x00").strip()
    cart_id = data[0x3C:0x3E].decode("ascii", errors="replace")
    country = chr(data[0x3E])
    version = data[0x3F]

    print(f"internal name : {name!r}")
    print(f"game code     : N{cart_id}{country}  (version {version})")
    print(f"clock rate    : {clock:#010x}")
    print(f"entry point   : {entry:#010x}")
    print(f"release/PC    : {release:#010x}")
    print(f"CRC1 / CRC2   : {crc1:#010x} / {crc2:#010x}")
    print()
    sha1 = hashlib.sha1(data).hexdigest()
    print(f"SHA-1 (.z64)  : {sha1}")
    print("expected      : b13072fef6c6df48c07d8822c01e5bc59036f6da (USA)"
          if sha1 != "b13072fef6c6df48c07d8822c01e5bc59036f6da" else "matches Yoshi's Story (USA)")


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <input rom> <output .z64 path>")
        sys.exit(1)

    with open(sys.argv[1], "rb") as f:
        data = f.read()

    normalized, note = normalize(data)
    print(f"format: {note}")
    print()
    print_header(normalized)

    with open(sys.argv[2], "wb") as f:
        f.write(normalized)
    print()
    print(f"wrote normalized ROM -> {sys.argv[2]}")


if __name__ == "__main__":
    main()
