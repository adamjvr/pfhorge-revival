#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import shutil
import stat
import tempfile
import unittest
import zipfile


REVIVAL_DIR = Path(__file__).resolve().parents[1]
TOOL = REVIVAL_DIR / "pfhorge_native.py"

spec = importlib.util.spec_from_file_location("pfhorge_native", TOOL)
assert spec and spec.loader
pf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pf)


class PfhorgeNativePackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = Path(tempfile.mkdtemp(prefix="pfhorge-native-test-"))

    def tearDown(self):
        shutil.rmtree(self.temp, ignore_errors=True)

    def sample(self, kind="level"):
        path = self.temp / ("sample.pfhlev" if kind == "level" else "sample.sen")
        pf.create_sample(path, kind=kind)
        return path

    def mutate_zip(self, source: Path, output: Path, mutator):
        with zipfile.ZipFile(source, "r") as src:
            pairs = [(i, src.read(i)) for i in src.infolist()]
        pairs = mutator(pairs)
        with zipfile.ZipFile(output, "w") as dst:
            for info, data in pairs:
                clone = zipfile.ZipInfo(info.filename, pf.DETERMINISTIC_ZIP_DATETIME)
                clone.compress_type = info.compress_type
                clone.create_system = info.create_system
                clone.external_attr = info.external_attr
                clone.flag_bits = info.flag_bits
                clone.extra = info.extra
                dst.writestr(clone, data)

    def test_sample_level_validates(self):
        path = self.sample("level")
        result = pf.validate_package(path)
        self.assertEqual(result["kind"], "level")
        self.assertEqual(result["levelCount"], 1)
        self.assertEqual(pf.identify(path), "pfhorge-native-vnext")

    def test_sample_scenario_validates(self):
        path = self.sample("scenario")
        result = pf.validate_package(path)
        self.assertEqual(result["kind"], "scenario")
        self.assertEqual(result["levelCount"], 2)

    def test_mimetype_is_first_and_stored(self):
        path = self.sample()
        with zipfile.ZipFile(path) as zf:
            infos = zf.infolist()
            self.assertEqual(infos[0].filename, "mimetype")
            self.assertEqual(infos[0].compress_type, zipfile.ZIP_STORED)
            self.assertEqual(infos[0].extra, b"")
            self.assertEqual(zf.read(infos[0]), pf.MIMETYPE)

    def test_unpack_repack_roundtrip_semantics(self):
        original = self.sample()
        unpacked = self.temp / "unpacked"
        repacked = self.temp / "repacked.pfhlev"
        pf.unpack(original, unpacked)
        pf.pack(unpacked, repacked, kind=None, compression="deflate")
        pf.validate_package(repacked)

        _, a = pf._read_zip_entries(original)
        _, b = pf._read_zip_entries(repacked)
        # ZIP compression differs but every logical package resource is identical.
        self.assertEqual(a, b)

    def test_hash_tamper_is_rejected(self):
        source = self.sample()
        bad = self.temp / "bad-hash.pfhlev"

        def mutate(pairs):
            result = []
            for info, data in pairs:
                if info.filename == "document.json":
                    doc = json.loads(data)
                    doc["title"] = "tampered"
                    data = pf.dump_json_bytes(doc)
                result.append((info, data))
            return result

        self.mutate_zip(source, bad, mutate)
        with self.assertRaises(pf.PackageError):
            pf.validate_package(bad)

    def test_wrong_first_entry_is_rejected(self):
        source = self.sample()
        bad = self.temp / "bad-order.pfhlev"

        with zipfile.ZipFile(source, "r") as src:
            entries = [(i.filename, src.read(i)) for i in src.infolist()]
        by_name = dict(entries)

        with zipfile.ZipFile(bad, "w") as dst:
            first = pf._zip_info("manifest.json", compression=zipfile.ZIP_STORED)
            dst.writestr(first, by_name["manifest.json"])
            for name, data in entries:
                if name == "manifest.json":
                    continue
                dst.writestr(
                    pf._zip_info(name, compression=zipfile.ZIP_STORED), data
                )

        with self.assertRaises(pf.PackageError):
            pf.validate_package(bad)

    def test_path_traversal_is_rejected(self):
        source = self.sample()
        bad = self.temp / "bad-path.pfhlev"
        with zipfile.ZipFile(source, "r") as src:
            entries = [(i.filename, src.read(i)) for i in src.infolist()]

        with zipfile.ZipFile(bad, "w") as dst:
            for name, data in entries:
                dst.writestr(
                    pf._zip_info(name, compression=zipfile.ZIP_STORED), data
                )
            dst.writestr(
                pf._zip_info("../escape.txt", compression=zipfile.ZIP_STORED),
                b"nope",
            )

        with self.assertRaises(pf.PackageError):
            pf.validate_package(bad)

    def test_symlink_entry_is_rejected(self):
        source = self.sample()
        bad = self.temp / "bad-link.pfhlev"
        with zipfile.ZipFile(source, "r") as src:
            entries = [(i.filename, src.read(i)) for i in src.infolist()]

        with zipfile.ZipFile(bad, "w") as dst:
            for name, data in entries:
                dst.writestr(
                    pf._zip_info(name, compression=zipfile.ZIP_STORED), data
                )
            info = zipfile.ZipInfo("link", pf.DETERMINISTIC_ZIP_DATETIME)
            info.create_system = 3
            info.external_attr = (stat.S_IFLNK | 0o777) << 16
            dst.writestr(info, b"/etc/passwd")

        with self.assertRaises(pf.PackageError):
            pf.validate_package(bad)

    def test_duplicate_json_member_is_rejected(self):
        with self.assertRaises(pf.PackageError):
            pf.load_json_bytes(b'{"a":1,"a":2}', "duplicate.json")

    def test_unsafe_integer_is_rejected(self):
        with self.assertRaises(pf.PackageError):
            pf.load_json_bytes(
                b'{"n":9007199254740992}', "unsafe-number.json"
            )

    def test_legacy_signature_identification(self):
        # version=4 is arbitrary here; identify() only needs the historical
        # Pfhorge signatures after it.
        import struct
        blob = struct.pack(">HHHI", 4, 26743, 34521, 42296737) + b"payload"
        path = self.temp / "legacy.pfhlev"
        path.write_bytes(blob)
        self.assertEqual(pf.identify(path), "pfhorge-legacy-pfhlev")


if __name__ == "__main__":
    unittest.main(verbosity=2)
