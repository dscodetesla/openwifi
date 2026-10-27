#!/usr/bin/env python3
"""Offline ONLY: prepare bootdelay 0->3 for the qualified nonredundant 128-KiB env."""
import argparse
import binascii
import pathlib
import struct

def prepare(data):
    if len(data) != 0x20000:
        raise ValueError("expected exactly 128 KiB")
    if struct.unpack("<I", data[:4])[0] != binascii.crc32(data[4:]) & 0xFFFFFFFF:
        raise ValueError("environment CRC mismatch or unsupported format")
    body = data[4:]
    # Search complete NUL-delimited variables, not substrings in another value.
    tokens = body.split(b"\0")
    positions = [i for i, token in enumerate(tokens) if token.startswith(b"bootdelay=")]
    if len(positions) != 1 or tokens[positions[0]] != b"bootdelay=0":
        raise ValueError("expected one bootdelay=0 variable")
    if b"\0\0" not in body:
        raise ValueError("environment terminator missing")
    tokens[positions[0]] = b"bootdelay=3"
    changed = b"\0".join(tokens)
    result = struct.pack("<I", binascii.crc32(changed) & 0xFFFFFFFF) + changed
    if len(result) != len(data) or sum(a != b for a, b in zip(data, result)) > 5:
        raise ValueError("unexpected edit size")
    return result

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("backup", type=pathlib.Path)
    ap.add_argument("output", type=pathlib.Path)
    args = ap.parse_args()
    result = prepare(args.backup.read_bytes())
    with args.output.open("xb") as f:
        f.write(result)
    print("PREPARED_OFFLINE_ONLY: bootdelay=3; no device was written")

if __name__ == "__main__":
    main()
