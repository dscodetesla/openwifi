#!/usr/bin/env python3
"""Artifact identity and fail-closed validation checks; no device access."""
import contextlib
import hashlib
import io
import json
import pathlib
import tempfile
import unittest
import preflight

class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name)
        self.fit = self.root / "fit"
        self.fit.write_bytes(b"test-image")
        self.manifest = {"schema_version": 1, "fit": {"bytes": 10, "fit_size_hex": "0xa",
                         "sha256": hashlib.sha256(b"test-image").hexdigest()}}
        self.mp = self.root / "manifest.json"

    def run_probe(self):
        self.mp.write_text(json.dumps(self.manifest))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = preflight.main(["--manifest", str(self.mp), "--fit", str(self.fit)])
        return code, json.loads(out.getvalue())

    def test_same_size_tamper_rejected(self):
        self.fit.write_bytes(b"evil-image")
        code, data = self.run_probe()
        self.assertEqual(code, 2)
        self.assertFalse(data["fit"]["match"])

    def test_truncation_rejected(self):
        self.fit.write_bytes(b"test")
        self.assertFalse(preflight.verify_fit(self.fit, self.manifest)["match"])

    def test_exact_bytes_accepted(self):
        self.assertTrue(preflight.verify_fit(self.fit, self.manifest)["match"])

    def test_size_hex_inconsistency_rejected(self):
        self.manifest["fit"]["fit_size_hex"] = "0xb"
        code, data = self.run_probe()
        self.assertEqual(code, 2)
        self.assertEqual(data["status"], "FAILED_PREFLIGHT")

    def test_missing_file_rejected_without_path_disclosure(self):
        self.fit.unlink()
        code, data = self.run_probe()
        self.assertEqual(code, 2)
        self.assertNotIn(str(self.root), json.dumps(data))

    def test_nonstring_hash_rejected(self):
        self.manifest["fit"]["sha256"] = ["a"] * 64
        code, data = self.run_probe()
        self.assertEqual(code, 2)
        self.assertEqual(data["status"], "FAILED_PREFLIGHT")

if __name__ == "__main__":
    unittest.main()
