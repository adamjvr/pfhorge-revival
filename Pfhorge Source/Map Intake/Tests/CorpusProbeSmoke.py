#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

from __future__ import annotations

import importlib.util
import struct
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODULE_PATH = ROOT / "scripts" / "revival" / "probe_map_corpus.py"
SPEC = importlib.util.spec_from_file_location("probe_map_corpus", MODULE_PATH)
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = module
SPEC.loader.exec_module(module)


def be16(value: int) -> bytes:
    return struct.pack(">H", value)


def be32(value: int) -> bytes:
    return struct.pack(">I", value)


def tag(name: bytes, payload_size: int, next_offset: int) -> bytes:
    return name + be32(next_offset) + be32(payload_size) + be32(0) + bytes(payload_size)


def make_map() -> bytes:
    first = tag(b"PNTS", 8, 24)
    second = tag(b"LINS", 32, 72)
    third = tag(b"POLY", 128, 0)
    entry = first + second + third
    directory_offset = 128 + len(entry)

    header = bytearray(128)
    struct.pack_into(">hh64sIihhhhI", header, 0, 4, 1, b"Corpus Smoke", 0,
                     directory_offset, 1, 74, 16, 10, 0)

    directory = bytearray(84)
    struct.pack_into(">iihhhI66s", directory, 0, 128, len(entry), 3, 0, 0, 1,
                     b"Smoke Level")
    return bytes(header) + entry + bytes(directory)


def make_sidecar() -> bytes:
    finder = b"sce2Pfhg" + bytes(24)
    header_size = 26 + 12
    header = bytearray(header_size)
    struct.pack_into(">II16sHIII", header, 0, module.APPLE_DOUBLE_MAGIC,
                     0x00020000, bytes(16), 1, module.APPLE_FINDER_INFO,
                     header_size, len(finder))
    return bytes(header) + finder


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_string:
        temp = Path(temp_string)
        archive_path = temp / "corpus.zip"
        with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("Smoke/Map", make_map())
            archive.writestr("Smoke/._Map", make_sidecar())

        records = module.scan_source(archive_path)
        assert len(records) == 1
        record = records[0]
        assert record.envelope == "appledouble-pair"
        assert record.finder_type == "sce2"
        assert record.recognized_container
        assert record.structurally_usable
        assert record.content_kind == "map"
        assert record.dialect == "infinity-compatible"
        assert record.level_names == ["Smoke Level"]

    print("MAP-1A corpus probe smoke test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
