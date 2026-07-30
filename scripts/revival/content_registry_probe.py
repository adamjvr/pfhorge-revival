#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Portable CONTENT-1A scanner used for fixtures and CI.

The macOS UI has an Objective-C scanner, while this script mirrors its public
classification rules so corpus/content fixtures can be validated on any host.
"""
from __future__ import annotations

import argparse
import json
import os
import stat
import zipfile
from dataclasses import asdict, dataclass, field
from pathlib import Path, PurePosixPath
from typing import Iterable

TEXTURE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".dds", ".tga", ".bmp", ".tif", ".tiff"}
SHAPES_EXTENSIONS = {".shpa", ".shp2", ".shp", ".shapes", ".shp∞"}
MAX_ENTRIES = 50_000
MAX_TOTAL_BYTES = 16 * 1024 * 1024 * 1024


def looks_like_shapes(name: str, size: int) -> bool:
    if size < 1024:
        return False
    path = PurePosixPath(name)
    lower = path.name.lower()
    extension = path.suffix.lower()
    if extension in SHAPES_EXTENSIONS:
        return True
    return (
        "shapes" in lower
        and extension not in {".txt", ".md", ".html", ".xml", ".mml"}
    )


def safe_archive_name(name: str) -> bool:
    if not name or name.startswith(("/", "\\")) or "\\" in name or "\x00" in name:
        return False
    parts = PurePosixPath(name).parts
    return ".." not in parts


@dataclass
class ScanReport:
    source: str
    source_kind: str
    shapes: list[str] = field(default_factory=list)
    mml: list[str] = field(default_factory=list)
    textures: list[str] = field(default_factory=list)
    plugins: list[str] = field(default_factory=list)
    builder_recipes: list[str] = field(default_factory=list)
    rights_documents: list[str] = field(default_factory=list)
    visited_entries: int = 0
    total_file_bytes: int = 0
    skipped_symlinks: int = 0
    truncated: bool = False
    unsafe_archive_entries: list[str] = field(default_factory=list)


def classify_path(name: str, size: int, report: ScanReport) -> None:
    normalized = name.replace(os.sep, "/")
    path = PurePosixPath(normalized)
    lower = path.name.lower()
    report.total_file_bytes += size
    if any(token in lower for token in ("license", "licence", "credits", "copying", "authors")):
        if len(report.rights_documents) < 64:
            report.rights_documents.append(normalized)
    if lower == "build_pack.py":
        if len(report.builder_recipes) < 32:
            report.builder_recipes.append(normalized)
        return
    if looks_like_shapes(normalized, size):
        if len(report.shapes) < 128:
            report.shapes.append(normalized)
        return
    if path.suffix.lower() == ".mml" or (
        path.suffix.lower() == ".xml" and "mml" in lower
    ):
        if len(report.mml) < 256:
            report.mml.append(normalized)
        return
    if path.suffix.lower() in TEXTURE_EXTENSIONS and len(report.textures) < 512:
        report.textures.append(normalized)


def scan_directory(root: Path) -> ScanReport:
    report = ScanReport(str(root), "directory")
    for current_root, dirs, files in os.walk(root, followlinks=False):
        current = Path(current_root)
        kept_dirs: list[str] = []
        for directory in dirs:
            candidate = current / directory
            try:
                mode = candidate.lstat().st_mode
            except OSError:
                continue
            if stat.S_ISLNK(mode):
                report.skipped_symlinks += 1
                continue
            lower = directory.lower()
            if lower == "plugins" or lower.endswith(".plugin") or "texture pack" in lower:
                if len(report.plugins) < 128:
                    report.plugins.append(str(candidate.relative_to(root)))
            kept_dirs.append(directory)
        dirs[:] = kept_dirs

        for filename in files:
            report.visited_entries += 1
            if report.visited_entries > MAX_ENTRIES:
                report.truncated = True
                return report
            path = current / filename
            try:
                st = path.lstat()
            except OSError:
                continue
            if stat.S_ISLNK(st.st_mode):
                report.skipped_symlinks += 1
                continue
            classify_path(str(path.relative_to(root)), st.st_size, report)
            if report.total_file_bytes > MAX_TOTAL_BYTES:
                report.truncated = True
                return report
    return report


def scan_zip(path: Path) -> ScanReport:
    report = ScanReport(str(path), "zip")
    casefolded_names: set[str] = set()
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            report.visited_entries += 1
            if report.visited_entries > MAX_ENTRIES:
                report.truncated = True
                break
            if not safe_archive_name(info.filename):
                report.unsafe_archive_entries.append(info.filename)
                continue
            folded = str(PurePosixPath(info.filename)).casefold()
            if folded in casefolded_names:
                report.unsafe_archive_entries.append(info.filename)
                continue
            casefolded_names.add(folded)
            mode = (info.external_attr >> 16) & 0xFFFF
            if stat.S_ISLNK(mode):
                report.skipped_symlinks += 1
                report.unsafe_archive_entries.append(info.filename)
                continue
            if info.is_dir():
                lower = PurePosixPath(info.filename.rstrip("/")).name.lower()
                if lower == "plugins" or lower.endswith(".plugin") or "texture pack" in lower:
                    report.plugins.append(info.filename)
                continue
            classify_path(info.filename, info.file_size, report)
            if report.total_file_bytes > MAX_TOTAL_BYTES:
                report.truncated = True
                report.unsafe_archive_entries.append("<expanded-size-limit>")
                break
    return report


def scan(path: Path) -> ScanReport:
    if path.is_dir():
        return scan_directory(path)
    if path.is_file() and zipfile.is_zipfile(path):
        return scan_zip(path)
    raise ValueError(f"unsupported content source: {path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    report = scan(args.source.expanduser().resolve())
    payload = asdict(report)
    encoded = json.dumps(payload, indent=2, sort_keys=True)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded + "\n", encoding="utf-8")
    else:
        print(encoded)

    useful = bool(report.shapes or report.mml or report.textures or report.builder_recipes)
    safe = not report.unsafe_archive_entries and not report.truncated
    return 0 if useful and safe else 2


if __name__ == "__main__":
    raise SystemExit(main())
