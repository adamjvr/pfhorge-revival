#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

"""Safely inventory Marathon-family map containers in a directory or ZIP.

The probe is deliberately read-only. It recognizes raw Marathon data forks,
AppleSingle, AppleDouble sidecars, and MacBinary. It validates the 128-byte
Marathon container header, directory records, and tag chains without creating
Pfhorge editor objects.
"""

from __future__ import annotations

import argparse
import binascii
import csv
import io
import json
import os
import struct
import sys
import zipfile
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field
from pathlib import Path, PurePosixPath
from typing import BinaryIO, Iterable, Iterator, Optional

APPLE_SINGLE_MAGIC = 0x00051600
APPLE_DOUBLE_MAGIC = 0x00051607
APPLEFILE_VERSIONS = {0x00010000, 0x00020000}
APPLE_DATA_FORK = 1
APPLE_RESOURCE_FORK = 2
APPLE_FINDER_INFO = 9
HEADER_SIZE = 128
MAX_SAFE_FILE_SIZE = 512 * 1024 * 1024
MAX_ENTRY_COUNT = 4096
MAX_TAGS_PER_ENTRY = 4096
MAP_TAGS = {b"PNTS", b"EPNT", b"LINS", b"POLY", b"SIDS"}


@dataclass
class Finding:
    severity: str
    message: str


@dataclass
class TagRecord:
    directory_ordinal: int
    logical_index: int
    tag: str
    payload_length: int
    patch_offset: int


@dataclass
class DirectoryRecord:
    directory_ordinal: int
    logical_index: int
    data_offset: int
    data_length: int
    entry_point_flags: Optional[int] = None
    level_name: Optional[str] = None
    valid: bool = False
    tags: list[TagRecord] = field(default_factory=list)


@dataclass
class ProbeRecord:
    path: str
    size: int
    sidecar_path: Optional[str] = None
    envelope: str = "raw"
    finder_type: Optional[str] = None
    resource_fork_size: int = 0
    recognized_container: bool = False
    structurally_usable: bool = False
    content_kind: str = "unknown"
    dialect: str = "unknown"
    container_version: Optional[int] = None
    data_version: Optional[int] = None
    internal_name: Optional[str] = None
    declared_checksum: Optional[str] = None
    computed_checksum: Optional[str] = None
    checksum_status: str = "not-calculated"
    parent_checksum: Optional[str] = None
    directory_offset: Optional[int] = None
    declared_entry_count: Optional[int] = None
    application_directory_data_size: Optional[int] = None
    entry_header_size: Optional[int] = None
    directory_entry_base_size: Optional[int] = None
    parsed_entry_count: int = 0
    directory_entries: list[DirectoryRecord] = field(default_factory=list)
    tags: list[str] = field(default_factory=list)
    level_names: list[str] = field(default_factory=list)
    findings: list[Finding] = field(default_factory=list)

    def add(self, severity: str, message: str) -> None:
        self.findings.append(Finding(severity, message))


@dataclass
class ForkSet:
    envelope: str
    data: bytes
    resource: bytes = b""
    finder_info: bytes = b""


@dataclass(frozen=True)
class SourceEntry:
    path: str
    size: int
    read_bytes: callable


def be16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def bes16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">h", data, offset)[0]


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def bes32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">i", data, offset)[0]


def decode_legacy_string(raw: bytes) -> str:
    raw = raw.split(b"\0", 1)[0]
    if not raw:
        return ""
    for encoding in ("mac_roman", "utf-8", "latin-1"):
        try:
            return raw.decode(encoding).strip()
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1", errors="replace").strip()


def fourcc(raw: bytes) -> str:
    return "".join(chr(value) if 0x20 <= value <= 0x7E else "." for value in raw)


def round_up_128(value: int) -> int:
    return (value + 127) & ~127


def parse_applefile(data: bytes) -> Optional[ForkSet]:
    if len(data) < 26:
        return None
    magic = be32(data, 0)
    if magic not in {APPLE_SINGLE_MAGIC, APPLE_DOUBLE_MAGIC}:
        return None
    version = be32(data, 4)
    if version not in APPLEFILE_VERSIONS:
        raise ValueError(f"unsupported AppleSingle/AppleDouble version 0x{version:08X}")
    count = be16(data, 24)
    descriptor_end = 26 + count * 12
    if descriptor_end > len(data):
        raise ValueError("AppleSingle/AppleDouble descriptor table is truncated")

    entries: dict[int, bytes] = {}
    for index in range(count):
        cursor = 26 + index * 12
        entry_id, offset, length = struct.unpack_from(">III", data, cursor)
        end = offset + length
        if end < offset or end > len(data):
            raise ValueError(f"Apple entry {entry_id} lies outside the file")
        entries[entry_id] = data[offset:end]

    return ForkSet(
        envelope="applesingle" if magic == APPLE_SINGLE_MAGIC else "appledouble",
        data=entries.get(APPLE_DATA_FORK, b""),
        resource=entries.get(APPLE_RESOURCE_FORK, b""),
        finder_info=entries.get(APPLE_FINDER_INFO, b""),
    )


def parse_macbinary(data: bytes) -> Optional[ForkSet]:
    if len(data) < 128:
        return None
    filename_length = data[1]
    if not (
        data[0] == 0
        and 1 <= filename_length <= 63
        and data[74] == 0
        and data[82] == 0
    ):
        return None

    data_length = be32(data, 83)
    resource_length = be32(data, 87)
    secondary_header_length = be16(data, 120)
    data_offset = 128 + round_up_128(secondary_header_length)
    resource_offset = data_offset + round_up_128(data_length)

    if data_offset + data_length > len(data):
        return None
    if resource_offset + resource_length > len(data):
        return None

    return ForkSet(
        envelope="macbinary",
        data=data[data_offset : data_offset + data_length],
        resource=data[resource_offset : resource_offset + resource_length],
        finder_info=data[65:81],
    )


def finder_type_from_info(finder_info: bytes) -> Optional[str]:
    if len(finder_info) < 4:
        return None
    return fourcc(finder_info[:4])


def resolve_forks(
    main_data: bytes,
    sidecar_data: Optional[bytes] = None,
    selected_is_sidecar: bool = False,
) -> ForkSet:
    parsed = parse_applefile(main_data)
    if parsed is not None:
        if parsed.envelope == "appledouble" and selected_is_sidecar:
            return parsed
        return parsed

    parsed = parse_macbinary(main_data)
    if parsed is not None:
        return parsed

    resource = b""
    finder_info = b""
    if sidecar_data:
        sidecar = parse_applefile(sidecar_data)
        if sidecar and sidecar.envelope == "appledouble":
            resource = sidecar.resource
            finder_info = sidecar.finder_info

    return ForkSet("appledouble-pair" if sidecar_data else "raw", main_data, resource, finder_info)


def calculate_checksum(data: bytes) -> int:
    mutable = bytearray(data)
    if len(mutable) >= 72:
        mutable[68:72] = b"\0\0\0\0"
    return binascii.crc32(mutable) & 0xFFFFFFFF


def classify_dialect(container_version: int, data_version: int) -> str:
    if data_version == 0:
        return "marathon-1"
    if data_version == 1:
        return "infinity-compatible" if container_version >= 4 else "marathon-2"
    if data_version == 2:
        return "aleph-one-extended"
    return "unknown"


def parse_tags(
    data: bytes,
    record: DirectoryRecord,
    container_version: int,
    stored_header_size: int,
    probe: ProbeRecord,
) -> list[TagRecord]:
    entry_start = record.data_offset
    entry_length = record.data_length
    header_size = 12 if container_version <= 1 else stored_header_size
    tags: list[TagRecord] = []

    if entry_length == 0:
        probe.add("warning", f"entry {record.directory_ordinal} has zero length")
        return tags

    relative = 0
    seen: set[int] = set()
    for _ in range(MAX_TAGS_PER_ENTRY):
        if relative in seen:
            probe.add("fatal", f"entry {record.directory_ordinal} has a cyclic tag chain")
            return tags
        seen.add(relative)

        absolute = entry_start + relative
        if relative < 0 or relative + header_size > entry_length:
            probe.add("fatal", f"entry {record.directory_ordinal} tag header leaves entry bounds")
            return tags
        if absolute < 0 or absolute + header_size > len(data):
            probe.add("fatal", f"entry {record.directory_ordinal} tag header leaves file bounds")
            return tags

        tag_raw = data[absolute : absolute + 4]
        next_offset = bes32(data, absolute + 4)
        payload_length = bes32(data, absolute + 8)
        patch_offset = bes32(data, absolute + 12) if header_size >= 16 else 0

        if payload_length < 0 or relative + header_size + payload_length > entry_length:
            probe.add("fatal", f"entry {record.directory_ordinal} tag payload leaves entry bounds")
            return tags

        tags.append(
            TagRecord(
                directory_ordinal=record.directory_ordinal,
                logical_index=record.logical_index,
                tag=fourcc(tag_raw),
                payload_length=payload_length,
                patch_offset=patch_offset,
            )
        )

        if next_offset == 0:
            return tags
        payload_end = relative + header_size + payload_length
        if next_offset < payload_end or next_offset >= entry_length:
            probe.add(
                "fatal",
                f"entry {record.directory_ordinal} has invalid next tag offset {next_offset}",
            )
            return tags
        relative = next_offset

    probe.add("fatal", f"entry {record.directory_ordinal} exceeds the tag safety limit")
    return tags


def probe_marathon_data(data: bytes, record: ProbeRecord) -> ProbeRecord:
    if len(data) < HEADER_SIZE:
        record.add("fatal", "data fork is shorter than the 128-byte Marathon header")
        return record

    (
        container_version,
        data_version,
        raw_name,
        declared_checksum,
        directory_offset,
        entry_count,
        app_size,
        entry_header_size,
        directory_base_size,
        parent_checksum,
    ) = struct.unpack_from(">hh64sIihhhhI", data, 0)

    record.container_version = container_version
    record.data_version = data_version
    record.internal_name = decode_legacy_string(raw_name)
    record.declared_checksum = f"0x{declared_checksum:08X}"
    record.parent_checksum = f"0x{parent_checksum:08X}"
    record.directory_offset = directory_offset
    record.declared_entry_count = entry_count
    record.application_directory_data_size = app_size
    record.entry_header_size = entry_header_size
    record.directory_entry_base_size = directory_base_size

    if container_version not in {0, 1, 2, 4}:
        record.add("fatal", f"unsupported Marathon container version {container_version}")
        return record
    if entry_count < 1 or entry_count > MAX_ENTRY_COUNT:
        record.add("fatal", f"implausible entry count {entry_count}")
        return record
    if directory_offset < HEADER_SIZE:
        record.add("fatal", f"directory offset {directory_offset} is before the header end")
        return record
    if app_size < 0:
        record.add("fatal", "negative application directory data size")
        return record

    if container_version <= 1:
        base_size = 8
        normalized_entry_header_size = 12
    else:
        base_size = directory_base_size
        normalized_entry_header_size = entry_header_size
        if not 8 <= base_size <= 64:
            record.add("fatal", f"unsafe directory base size {base_size}")
            return record
        if not 12 <= normalized_entry_header_size <= 64:
            record.add("fatal", f"unsafe entry header size {normalized_entry_header_size}")
            return record

    record_size = base_size + app_size
    directory_end = directory_offset + entry_count * record_size
    if record_size < base_size or directory_end < directory_offset or directory_end > len(data):
        record.add("fatal", "directory extends outside the data fork")
        return record

    record.recognized_container = True
    record.dialect = classify_dialect(container_version, data_version)
    record.computed_checksum = f"0x{calculate_checksum(data):08X}"
    computed_int = int(record.computed_checksum, 16)
    if declared_checksum == 0:
        record.checksum_status = "not-present"
    elif declared_checksum == computed_int:
        record.checksum_status = "matches"
    else:
        record.checksum_status = "mismatch"
        record.add(
            "warning",
            f"stored checksum 0x{declared_checksum:08X} does not match computed 0x{computed_int:08X}",
        )

    all_tags: list[TagRecord] = []
    level_names: list[str] = []
    seen_ranges: list[tuple[int, int, int, int]] = []
    for ordinal in range(entry_count):
        cursor = directory_offset + ordinal * record_size
        data_offset = bes32(data, cursor)
        data_length = bes32(data, cursor + 4)
        logical_index = bes16(data, cursor + 8) if container_version >= 2 and base_size >= 10 else ordinal

        entry = DirectoryRecord(ordinal, logical_index, data_offset, data_length)
        if app_size >= 74:
            app = cursor + base_size
            entry.entry_point_flags = be32(data, app + 4)
            entry.level_name = decode_legacy_string(data[app + 8 : app + 74]) or None
            if entry.level_name:
                level_names.append(entry.level_name)

        if data_offset < HEADER_SIZE or data_length < 0 or data_offset + data_length > len(data):
            record.add("fatal", f"directory entry {ordinal} points outside the data fork")
            record.directory_entries.append(entry)
            continue

        entry_end = data_offset + data_length
        if data_offset < directory_end and entry_end > directory_offset:
            record.add("fatal", f"directory entry {ordinal} overlaps the directory table")

        for previous_ordinal, previous_index, previous_start, previous_end in seen_ranges:
            if data_offset < previous_end and entry_end > previous_start:
                record.add(
                    "fatal",
                    f"directory entries {previous_ordinal} and {ordinal} overlap",
                )
            if previous_index == logical_index:
                record.add(
                    "warning",
                    f"directory entries {previous_ordinal} and {ordinal} share logical index {logical_index}",
                )

        seen_ranges.append((ordinal, logical_index, data_offset, entry_end))
        entry.valid = not any(
            finding.severity == "fatal"
            and f"entry {ordinal}" in finding.message
            for finding in record.findings
        )
        entry.tags = parse_tags(
            data,
            entry,
            container_version,
            normalized_entry_header_size,
            record,
        )
        record.directory_entries.append(entry)
        all_tags.extend(entry.tags)

    unique_tags = sorted({tag.tag for tag in all_tags})
    record.tags = unique_tags
    record.level_names = level_names
    record.parsed_entry_count = entry_count

    tag_bytes = {tag.encode("latin-1", errors="replace") for tag in unique_tags}
    has_points = b"PNTS" in tag_bytes or b"EPNT" in tag_bytes
    if has_points and b"LINS" in tag_bytes and b"POLY" in tag_bytes:
        record.content_kind = "map"
    elif unique_tags:
        record.content_kind = "non-map-marathon-container"
        record.add("warning", "Marathon container lacks a complete map geometry tag set")
    else:
        record.content_kind = "unknown-marathon-container"

    record.structurally_usable = not any(f.severity == "fatal" for f in record.findings)
    return record


def iter_filesystem_entries(root: Path) -> Iterator[SourceEntry]:
    if root.is_file():
        yield SourceEntry(str(root), root.stat().st_size, root.read_bytes)
        return
    for path in root.rglob("*"):
        if path.is_file():
            rel = path.relative_to(root).as_posix()
            yield SourceEntry(rel, path.stat().st_size, path.read_bytes)


def iter_zip_entries(zip_path: Path) -> Iterator[SourceEntry]:
    archive = zipfile.ZipFile(zip_path)
    for info in archive.infolist():
        if info.is_dir():
            continue
        name = PurePosixPath(info.filename).as_posix()

        def reader(info: zipfile.ZipInfo = info, archive: zipfile.ZipFile = archive) -> bytes:
            with archive.open(info, "r") as source:
                return source.read(MAX_SAFE_FILE_SIZE + 1)

        yield SourceEntry(name, info.file_size, reader)


def sidecar_name(path: str) -> str:
    pure = PurePosixPath(path)
    return str(pure.with_name("._" + pure.name))


def main_name_from_sidecar(path: str) -> Optional[str]:
    pure = PurePosixPath(path)
    if not pure.name.startswith("._"):
        return None
    return str(pure.with_name(pure.name[2:]))


def scan_source(source: Path) -> list[ProbeRecord]:
    entries = list(iter_zip_entries(source) if zipfile.is_zipfile(source) else iter_filesystem_entries(source))
    by_path = {entry.path: entry for entry in entries}
    records: list[ProbeRecord] = []

    ignored_names = {"MAP_INDEX.csv", "README_MAPS_ONLY.txt"}

    for entry in entries:
        pure = PurePosixPath(entry.path)
        if pure.name in ignored_names:
            continue
        if pure.name.startswith("._"):
            continue

        record = ProbeRecord(path=entry.path, size=entry.size)
        if entry.size == 0:
            record.add("warning", "zero-byte file or classic alias placeholder")
            records.append(record)
            continue
        if entry.size > MAX_SAFE_FILE_SIZE:
            record.add("fatal", f"file exceeds the {MAX_SAFE_FILE_SIZE}-byte safety limit")
            records.append(record)
            continue

        try:
            main_data = entry.read_bytes()
            if len(main_data) > MAX_SAFE_FILE_SIZE:
                raise ValueError("expanded file exceeds safety limit")

            sidecar_entry = by_path.get(sidecar_name(entry.path))
            sidecar_data = sidecar_entry.read_bytes() if sidecar_entry and sidecar_entry.size <= MAX_SAFE_FILE_SIZE else None
            if sidecar_entry:
                record.sidecar_path = sidecar_entry.path

            forks = resolve_forks(main_data, sidecar_data)
            record.envelope = forks.envelope
            record.resource_fork_size = len(forks.resource)
            record.finder_type = finder_type_from_info(forks.finder_info)

            if not forks.data:
                record.add("fatal", "selected envelope contains no data fork")
                records.append(record)
                continue

            probe_marathon_data(forks.data, record)
        except (OSError, ValueError, struct.error, zipfile.BadZipFile) as error:
            record.add("fatal", str(error))

        records.append(record)

    return records


def write_json(records: list[ProbeRecord], path: Path) -> None:
    path.write_text(json.dumps([asdict(record) for record in records], indent=2, ensure_ascii=False) + "\n")


def write_csv(records: list[ProbeRecord], path: Path) -> None:
    fieldnames = [
        "path",
        "size",
        "sidecar_path",
        "envelope",
        "finder_type",
        "resource_fork_size",
        "recognized_container",
        "structurally_usable",
        "content_kind",
        "dialect",
        "container_version",
        "data_version",
        "internal_name",
        "declared_checksum",
        "computed_checksum",
        "checksum_status",
        "parent_checksum",
        "declared_entry_count",
        "parsed_entry_count",
        "logical_indexes",
        "tags",
        "level_names",
        "findings",
    ]
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames)
        writer.writeheader()
        for record in records:
            row = asdict(record)
            row["logical_indexes"] = " ".join(
                str(entry.logical_index) for entry in record.directory_entries
            )
            row["tags"] = " ".join(record.tags)
            row["level_names"] = " | ".join(record.level_names)
            row["findings"] = " | ".join(f"{f.severity}: {f.message}" for f in record.findings)
            writer.writerow({key: row.get(key) for key in fieldnames})


def write_markdown(records: list[ProbeRecord], path: Path, source: Path) -> None:
    dialects = Counter(record.dialect for record in records if record.recognized_container)
    versions = Counter(
        (record.container_version, record.data_version)
        for record in records
        if record.recognized_container
    )
    envelopes = Counter(record.envelope for record in records)
    content = Counter(record.content_kind for record in records)
    fatal = [record for record in records if any(f.severity == "fatal" for f in record.findings)]
    usable_maps = [record for record in records if record.content_kind == "map" and record.structurally_usable]

    lines = [
        "# Marathon Map Corpus Probe",
        "",
        f"Source: `{source}`",
        "",
        "## Summary",
        "",
        f"- Candidate data files: **{len(records)}**",
        f"- Recognized Marathon containers: **{sum(r.recognized_container for r in records)}**",
        f"- Structurally usable maps: **{len(usable_maps)}**",
        f"- Files with fatal findings: **{len(fatal)}**",
        "",
        "## Data dialects",
        "",
        "| Dialect | Count |",
        "|---|---:|",
    ]
    lines.extend(f"| {name} | {count} |" for name, count in sorted(dialects.items()))
    lines.extend(["", "## Container/data versions", "", "| Container | Data | Count |", "|---:|---:|---:|"])
    lines.extend(f"| {container} | {data} | {count} |" for (container, data), count in sorted(versions.items()))
    lines.extend(["", "## Source envelopes", "", "| Envelope | Count |", "|---|---:|"])
    lines.extend(f"| {name} | {count} |" for name, count in sorted(envelopes.items()))
    lines.extend(["", "## Content classification", "", "| Content | Count |", "|---|---:|"])
    lines.extend(f"| {name} | {count} |" for name, count in sorted(content.items()))

    lines.extend(["", "## Fatal findings", ""])
    if not fatal:
        lines.append("No fatal findings.")
    else:
        lines.extend(["| Path | Findings |", "|---|---|"])
        for record in fatal[:200]:
            messages = "; ".join(f.message for f in record.findings if f.severity == "fatal")
            lines.append(f"| `{record.path}` | {messages.replace('|', '\\|')} |")
        if len(fatal) > 200:
            lines.append(f"\nOnly the first 200 of {len(fatal)} fatal records are shown here; see JSON/CSV for all results.")

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Map corpus directory, individual file, or ZIP archive")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("RevivalArtifacts"),
        help="Output directory (default: RevivalArtifacts)",
    )
    return parser.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    if not args.source.exists():
        print(f"error: source does not exist: {args.source}", file=sys.stderr)
        return 2

    args.output_dir.mkdir(parents=True, exist_ok=True)
    records = scan_source(args.source)

    json_path = args.output_dir / "map-corpus-report.json"
    csv_path = args.output_dir / "map-corpus-report.csv"
    md_path = args.output_dir / "map-corpus-summary.md"
    write_json(records, json_path)
    write_csv(records, csv_path)
    write_markdown(records, md_path, args.source)

    print(f"Scanned {len(records)} candidate data files")
    print(f"Recognized {sum(r.recognized_container for r in records)} Marathon containers")
    print(f"Usable maps {sum(r.content_kind == 'map' and r.structurally_usable for r in records)}")
    print(json_path)
    print(csv_path)
    print(md_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
