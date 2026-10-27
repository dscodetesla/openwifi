#!/usr/bin/env python3
"""Validate the pinned R2 DFU suffix and extract a FIT, without overwriting files."""
import argparse
import binascii
import hashlib
import json
import pathlib
import struct

def extract(data):
    if len(data) < 56:
        raise ValueError("DFU too short")
    suffix = struct.unpack("<HHHH3sB", data[-16:-4])
    if suffix != (0xFFFF, 0xB673, 0x0456, 0x0100, b"UFD", 16):
        raise ValueError("unexpected R2 DFU suffix")
    expected = (binascii.crc32(data[:-4]) ^ 0xFFFFFFFF) & 0xFFFFFFFF
    if struct.unpack("<I", data[-4:])[0] != expected:
        raise ValueError("DFU CRC mismatch")
    fit = data[:-16]
    magic, total = struct.unpack(">II", fit[:8])
    if magic != 0xD00DFEED or total != len(fit) or not 40 <= total <= 0x1E00000:
        raise ValueError("invalid FIT header/size")
    return fit

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dfu", type=pathlib.Path)
    ap.add_argument("output", type=pathlib.Path)
    args = ap.parse_args()
    fit = extract(args.dfu.read_bytes())
    with args.output.open("xb") as f:
        f.write(fit)
    print(json.dumps({"bytes": len(fit), "fit_size_hex": hex(len(fit)),
                      "sha256": hashlib.sha256(fit).hexdigest(),
                      "crc32": format(binascii.crc32(fit) & 0xFFFFFFFF, "08x")}, indent=2))

if __name__ == "__main__":
    main()
