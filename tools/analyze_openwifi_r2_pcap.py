#!/usr/bin/env python3
"""Offline check for classic PCAP from the recorded Openwifi R2 FPGA format.

The FPGA replaces the last FCS octet with status/sequence. This checks the
preserved 24 CRC bits and radiotap/status agreement; it does not restore FCS
or claim a pristine 32-bit CRC pass. No addresses or payloads are printed.
"""
import argparse
import binascii
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct


def analyze(data):
    formats = {b"\xd4\xc3\xb2\xa1": "<", b"\xa1\xb2\xc3\xd4": ">",
               b"\x4d\x3c\xb2\xa1": "<", b"\xa1\xb2\x3c\x4d": ">"}
    if len(data) < 24 or data[:4] not in formats:
        raise ValueError("Expected classic PCAP, not pcapng")
    endian = formats[data[:4]]
    if struct.unpack_from(endian + "HH", data, 4) != (2, 4):
        raise ValueError("Unsupported PCAP version")
    snaplen = struct.unpack_from(endian + "I", data, 16)[0]
    if not snaplen:
        raise ValueError("Invalid PCAP snaplen")
    if struct.unpack_from(endian + "I", data, 20)[0] != 127:
        raise ValueError("Expected IEEE802_11_RADIO link type 127")
    counts, frequencies = Counter(), Counter()
    offset = 24
    while offset < len(data):
        if offset + 16 > len(data):
            raise ValueError("Incomplete PCAP record header")
        _, _, size, original_size = struct.unpack_from(endian + "IIII", data, offset)
        offset += 16
        if size > snaplen or size > original_size:
            raise ValueError("Invalid PCAP captured length")
        if offset + size > len(data):
            raise ValueError("Incomplete PCAP record payload")
        packet = data[offset:offset + size]
        offset += size
        counts["records"] += 1
        if size != original_size:
            counts["truncated_records"] += 1
            continue
        if len(packet) < 8 or packet[0] != 0:
            raise ValueError("Invalid radiotap header")
        rtlen = struct.unpack_from("<H", packet, 2)[0]
        first = present = struct.unpack_from("<I", packet, 4)[0]
        position = 8
        while present & 0x80000000:
            if position + 4 > rtlen:
                raise ValueError("Truncated radiotap present words")
            present = struct.unpack_from("<I", packet, position)[0]
            position += 4
        if rtlen > len(packet) or position > rtlen:
            raise ValueError("Invalid radiotap length")
        if first & 1:  # TSFT, aligned to 8 bytes
            position = ((position + 7) & ~7) + 8
        if not first & 2:
            counts["missing_radiotap_flags"] += 1
            continue
        if position >= rtlen:
            raise ValueError("Truncated radiotap flags")
        flags = packet[position]
        position += 1
        if first & 4:  # legacy rate
            if position >= rtlen:
                raise ValueError("Truncated radiotap rate")
            position += 1
        if first & 8:  # channel, aligned to 2 bytes
            position = (position + 1) & ~1
            if position + 4 > rtlen:
                raise ValueError("Truncated channel field")
            frequencies[str(struct.unpack_from("<H", packet, position)[0])] += 1
        frame = packet[rtlen:]
        if not flags & 0x10 or len(frame) < 4:
            counts["missing_fcs_trailer"] += 1
            continue
        radiotap_good = not bool(flags & 0x40)
        fpga_good = bool(frame[-1] & 0x80)
        expected = (binascii.crc32(frame[:-4]) & 0xffffffff).to_bytes(4, "little")
        crc24_match = frame[-4:-1] == expected[:3]
        counts["radiotap_good" if radiotap_good else "radiotap_bad"] += 1
        counts["fpga_good" if fpga_good else "fpga_bad"] += 1
        counts["crc24_match" if crc24_match else "crc24_mismatch"] += 1
        counts["metadata_agrees" if radiotap_good == fpga_good else "metadata_mismatches"] += 1
        if fpga_good and not crc24_match:
            counts["good_status_crc24_mismatch"] += 1
    for name in ("metadata_mismatches", "good_status_crc24_mismatch", "truncated_records"):
        counts.setdefault(name, 0)
    return {"counts": dict(counts), "frequencies_mhz": dict(frequencies),
            "pcap_sha256": hashlib.sha256(data).hexdigest(),
            "scope": "Openwifi R2 status/sequence trailer; preserved CRC24 only"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcap", type=Path)
    parser.add_argument("--expect-consistent", action="store_true")
    args = parser.parse_args()
    try:
        result = analyze(args.pcap.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        parser.exit(2, str(error) + "\n")
    print(json.dumps(result, indent=2, sort_keys=True))
    counts = result["counts"]
    if args.expect_consistent and (not counts.get("fpga_good", 0) or
                                  counts["metadata_mismatches"] or
                                  counts["good_status_crc24_mismatch"] or
                                  counts["truncated_records"] or
                                  counts.get("missing_fcs_trailer", 0) or
                                  counts.get("missing_radiotap_flags", 0)):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
