#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

SCRIPT = (
    Path(__file__).resolve().parents[1]
    / "audit_pfhlev_tree.py"
)

spec = importlib.util.spec_from_file_location(
    "audit_pfhlev_tree",
    SCRIPT,
)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(mod)

MIME = b"application/vnd.pfhorge.package+zip"

class Format3AAuditTests(unittest.TestCase):
    def test_native(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "x.pfhlev"
            with zipfile.ZipFile(
                path,
                "w",
                zipfile.ZIP_STORED,
            ) as zf:
                zf.writestr("mimetype", MIME)
                zf.writestr("manifest.json", b"{}")
            self.assertEqual(
                mod.classify(path)[0],
                "native",
            )

    def test_legacy(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "x.pfhlev"
            header = (
                (7).to_bytes(2, "big")
                + (26743).to_bytes(2, "big")
                + (34521).to_bytes(2, "big")
                + (42296737).to_bytes(4, "big")
            )
            path.write_bytes(header + b"bplist00")
            self.assertEqual(
                mod.classify(path)[0],
                "legacy",
            )

    def test_unrelated_zip(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "x.pfhlev"
            with zipfile.ZipFile(
                path,
                "w",
                zipfile.ZIP_STORED,
            ) as zf:
                zf.writestr("other", b"x")
            self.assertEqual(
                mod.classify(path)[0],
                "unknown",
            )

if __name__ == "__main__":
    unittest.main(verbosity=2)
