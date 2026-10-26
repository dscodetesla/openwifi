"""Small synthetic format/metadata regressions; no captured traffic included."""
import binascii
import importlib.util
from pathlib import Path
import struct
import unittest

SPEC = importlib.util.spec_from_file_location(
    "r2_pcap", Path(__file__).parents[1] / "analyze_openwifi_r2_pcap.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def capture(rtlen=10, flags=0x10, include_rate=True):
    body = b"\xd4\x00" + bytes(8)
    frame = body + (binascii.crc32(body) & 0xffffffff).to_bytes(4, "little")[:3] + b"\x80"
    rt = struct.pack("<BBHI", 0, 0, rtlen, 6) + bytes([flags])
    if include_rate:
        rt += b"\x0c"
    packet = rt + frame
    header = struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 127)
    return header + struct.pack("<IIII", 0, 0, len(packet), len(packet)) + packet


class CaptureChecks(unittest.TestCase):
    def test_valid_status_sequence_trailer(self):
        counts = MODULE.analyze(capture())["counts"]
        self.assertEqual(counts["fpga_good"], 1)
        self.assertEqual(counts["crc24_match"], 1)
        self.assertEqual(counts["metadata_mismatches"], 0)

    def test_stale_radiotap_badfcs_is_reported(self):
        counts = MODULE.analyze(capture(flags=0x50))["counts"]
        self.assertEqual(counts["metadata_mismatches"], 1)

    def test_present_rate_requires_its_byte(self):
        with self.assertRaisesRegex(ValueError, "radiotap rate"):
            MODULE.analyze(capture(rtlen=9, include_rate=False))

    def test_incomplete_record_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Incomplete PCAP record"):
            MODULE.analyze(capture()[:-1])


if __name__ == "__main__":
    unittest.main()
