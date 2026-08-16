#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Reference tooling for the Pfhorge Native vNext FORMAT-1A package.

This is intentionally standard-library-only.  It is both useful tooling and an
executable description of the package safety/determinism rules while the native
Cocoa codec is being designed.

Commands:
    identify PATH
    inspect PATH
    validate PATH
    unpack PACKAGE DEST
    pack DIRECTORY PACKAGE [--kind ...] [--compression store|deflate]
    create-sample PACKAGE [--kind level|scenario]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import sys
import tempfile
import uuid
import zipfile
from typing import Any, Iterable

MIMETYPE = b"application/vnd.pfhorge.package+zip"
FORMAT_ID = "org.pfhorge.native"
FORMAT_VERSION = "1.0.0-draft.1"
MANIFEST_SCHEMA = "urn:pfhorge:schema:manifest:1"
DOCUMENT_SCHEMA = "urn:pfhorge:schema:document:1"
LEVEL_SCHEMA = "urn:pfhorge:schema:level:1"

MAX_ENTRIES = 4096
MAX_ENTRY_BYTES = 64 * 1024 * 1024
MAX_TOTAL_BYTES = 256 * 1024 * 1024
MAX_SAFE_JSON_INTEGER = 9007199254740991

# DOS ZIP timestamps cannot represent dates before 1980.
DETERMINISTIC_ZIP_DATETIME = (1980, 1, 1, 0, 0, 0)
REGULAR_FILE_MODE = 0o100644


class PackageError(Exception):
    pass


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _reject_constant(value: str) -> None:
    raise ValueError(f"non-finite JSON number is forbidden: {value}")


def _pairs_no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    obj: dict[str, Any] = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError(f"duplicate JSON object member: {key!r}")
        obj[key] = value
    return obj


def load_json_bytes(data: bytes, label: str) -> Any:
    if data.startswith(b"\xef\xbb\xbf"):
        raise PackageError(f"{label}: UTF-8 BOM is forbidden")
    try:
        text = data.decode("utf-8", errors="strict")
    except UnicodeDecodeError as exc:
        raise PackageError(f"{label}: invalid UTF-8: {exc}") from exc

    try:
        value = json.loads(
            text,
            object_pairs_hook=_pairs_no_duplicates,
            parse_constant=_reject_constant,
        )
    except (ValueError, json.JSONDecodeError) as exc:
        raise PackageError(f"{label}: invalid JSON: {exc}") from exc

    validate_json_value(value, label)
    return value


def validate_json_value(value: Any, path: str = "$") -> None:
    """
    Enforce the interoperable I-JSON subset used by the draft.

    Python's bool is a subclass of int, so booleans must be checked first.
    """
    if value is None or isinstance(value, (str, bool)):
        return
    if isinstance(value, int):
        if abs(value) > MAX_SAFE_JSON_INTEGER:
            raise PackageError(
                f"{path}: integer {value} exceeds the interoperable JSON "
                f"safe range; represent exact wider integers as strings"
            )
        return
    if isinstance(value, float):
        if not math.isfinite(value):
            raise PackageError(f"{path}: NaN/Infinity is forbidden")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            validate_json_value(item, f"{path}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            if not isinstance(key, str):
                raise PackageError(f"{path}: JSON object keys must be strings")
            validate_json_value(item, f"{path}.{key}")
        return
    raise PackageError(f"{path}: unsupported JSON value {type(value).__name__}")


def dump_json_bytes(value: Any) -> bytes:
    validate_json_value(value)
    # Human-readable deterministic form.  We intentionally do not claim JCS:
    # JCS removes insignificant whitespace and has specific ECMAScript number
    # serialization rules.  The package manifest hashes these exact bytes.
    return (
        json.dumps(
            value,
            ensure_ascii=False,
            allow_nan=False,
            indent=2,
            sort_keys=True,
            separators=(",", ": "),
        )
        + "\n"
    ).encode("utf-8")


def canonical_uuid(value: str, label: str) -> str:
    try:
        parsed = uuid.UUID(value)
    except (ValueError, AttributeError) as exc:
        raise PackageError(f"{label}: invalid UUID {value!r}") from exc
    canonical = str(parsed)
    if value != canonical:
        raise PackageError(
            f"{label}: UUID must use canonical lowercase RFC textual form: {canonical}"
        )
    return canonical


def safe_package_path(name: str) -> str:
    if not name:
        raise PackageError("empty ZIP/package path is forbidden")
    if "\\" in name:
        raise PackageError(f"backslash is forbidden in package path: {name!r}")
    if name.startswith("/"):
        raise PackageError(f"absolute package path is forbidden: {name!r}")
    # PurePosixPath normalizes repeated slashes; reject them before that so
    # two different archive spellings cannot alias the same logical path.
    if "//" in name:
        raise PackageError(f"repeated slash is forbidden in package path: {name!r}")
    parts = name.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise PackageError(f"unsafe path component in package path: {name!r}")
    if ":" in parts[0]:
        # Avoid drive-letter-like ambiguity when an archive is processed on Windows.
        raise PackageError(f"drive/scheme-like package path is forbidden: {name!r}")
    return str(PurePosixPath(name))


def _is_symlink(info: zipfile.ZipInfo) -> bool:
    # UNIX file type is carried in the high 16 bits when create_system == 3.
    if info.create_system != 3:
        return False
    mode = (info.external_attr >> 16) & 0xFFFF
    return stat.S_ISLNK(mode)


def _zip_info(name: str, *, compression: int) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(filename=name, date_time=DETERMINISTIC_ZIP_DATETIME)
    info.compress_type = compression
    info.create_system = 3
    info.external_attr = REGULAR_FILE_MODE << 16
    info.extra = b""
    info.comment = b""
    return info


def _read_zip_entries(
    package: Path,
    *,
    max_entries: int = MAX_ENTRIES,
    max_entry_bytes: int = MAX_ENTRY_BYTES,
    max_total_bytes: int = MAX_TOTAL_BYTES,
) -> tuple[list[zipfile.ZipInfo], dict[str, bytes]]:
    try:
        zf = zipfile.ZipFile(package, "r")
    except (OSError, zipfile.BadZipFile) as exc:
        raise PackageError(f"{package}: not a readable ZIP package: {exc}") from exc

    with zf:
        infos = zf.infolist()
        if not infos:
            raise PackageError("package contains no ZIP entries")
        if len(infos) > max_entries:
            raise PackageError(
                f"package has {len(infos)} entries; safety limit is {max_entries}"
            )

        if infos[0].filename != "mimetype":
            raise PackageError("mimetype MUST be the first ZIP entry")

        seen: set[str] = set()
        total = 0
        entries: dict[str, bytes] = {}

        for info in infos:
            name = safe_package_path(info.filename)
            if name in seen:
                raise PackageError(f"duplicate package path: {name!r}")
            seen.add(name)

            if info.is_dir():
                raise PackageError(
                    f"explicit directory entries are not used by the Pfhorge profile: {name!r}"
                )
            if _is_symlink(info):
                raise PackageError(f"symbolic link entry is forbidden: {name!r}")
            if info.flag_bits & 0x1:
                raise PackageError(f"encrypted ZIP entry is forbidden: {name!r}")
            if info.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
                raise PackageError(
                    f"unsupported ZIP method {info.compress_type} for {name!r}; "
                    "only Store and Deflate are allowed"
                )
            if info.file_size > max_entry_bytes:
                raise PackageError(
                    f"{name!r} is {info.file_size} bytes; per-entry safety limit "
                    f"is {max_entry_bytes}"
                )
            total += info.file_size
            if total > max_total_bytes:
                raise PackageError(
                    f"uncompressed package total exceeds {max_total_bytes} bytes"
                )

            # Reading after the advertised size checks is important: Python's zipfile
            # validates CRC while streaming/decompressing the member.
            try:
                data = zf.read(info)
            except (RuntimeError, zipfile.BadZipFile, OSError) as exc:
                raise PackageError(f"failed to read {name!r}: {exc}") from exc
            if len(data) != info.file_size:
                raise PackageError(
                    f"{name!r}: extracted length does not match ZIP directory"
                )
            entries[name] = data

        mime = infos[0]
        if mime.compress_type != zipfile.ZIP_STORED:
            raise PackageError("mimetype MUST be stored uncompressed")
        if mime.extra:
            raise PackageError("mimetype ZIP header MUST NOT contain extra data")
        if entries["mimetype"] != MIMETYPE:
            raise PackageError(
                "mimetype contents do not identify a Pfhorge Native package"
            )

        return infos, entries


def _validate_extension_decl(ext: Any, label: str) -> None:
    if not isinstance(ext, dict):
        raise PackageError(f"{label}: extension declaration must be an object")
    required = ("id", "version", "requiredForRead", "requiredForWrite")
    for key in required:
        if key not in ext:
            raise PackageError(f"{label}: missing {key!r}")
    if not isinstance(ext["id"], str) or "." not in ext["id"]:
        raise PackageError(f"{label}.id: expected reverse-DNS-like identifier")
    if not isinstance(ext["version"], str) or not ext["version"]:
        raise PackageError(f"{label}.version: expected non-empty string")
    if not isinstance(ext["requiredForRead"], bool):
        raise PackageError(f"{label}.requiredForRead: expected boolean")
    if not isinstance(ext["requiredForWrite"], bool):
        raise PackageError(f"{label}.requiredForWrite: expected boolean")


def _validate_manifest(manifest: Any, *, require_resources: bool = True) -> None:
    if not isinstance(manifest, dict):
        raise PackageError("manifest.json root must be a JSON object")

    required = {
        "$schema",
        "format",
        "formatVersion",
        "kind",
        "document",
        "extensions",
        "resources",
    }
    missing = sorted(required - manifest.keys())
    if missing:
        raise PackageError(f"manifest.json missing required fields: {', '.join(missing)}")

    if manifest["$schema"] != MANIFEST_SCHEMA:
        raise PackageError("manifest.json uses an unsupported schema identifier")
    if manifest["format"] != FORMAT_ID:
        raise PackageError("manifest.json format is not org.pfhorge.native")
    if manifest["kind"] not in ("level", "scenario"):
        raise PackageError("manifest.json kind must be 'level' or 'scenario'")
    if not isinstance(manifest["formatVersion"], str):
        raise PackageError("manifest.json formatVersion must be a string")
    safe_package_path(manifest["document"])

    if not isinstance(manifest["extensions"], list):
        raise PackageError("manifest.json extensions must be an array")
    ids: set[str] = set()
    for index, ext in enumerate(manifest["extensions"]):
        _validate_extension_decl(ext, f"manifest.extensions[{index}]")
        if ext["id"] in ids:
            raise PackageError(f"duplicate extension declaration: {ext['id']}")
        ids.add(ext["id"])

    if not isinstance(manifest["resources"], list):
        raise PackageError("manifest.json resources must be an array")
    if require_resources and not manifest["resources"]:
        raise PackageError("manifest.json resources must be a non-empty array")


def _validate_document(document: Any, manifest: dict[str, Any]) -> list[tuple[str, str]]:
    if not isinstance(document, dict):
        raise PackageError("document.json root must be an object")
    required = ("$schema", "id", "kind", "levels", "extensions")
    for key in required:
        if key not in document:
            raise PackageError(f"document.json missing required field {key!r}")
    if document["$schema"] != DOCUMENT_SCHEMA:
        raise PackageError("document.json uses an unsupported schema identifier")
    canonical_uuid(document["id"], "document.id")
    if document["kind"] != manifest["kind"]:
        raise PackageError("document kind disagrees with manifest kind")
    if not isinstance(document["levels"], list) or not document["levels"]:
        raise PackageError("document.levels must be a non-empty array")
    if manifest["kind"] == "level" and len(document["levels"]) != 1:
        raise PackageError("a kind='level' package must contain exactly one level")
    if not isinstance(document["extensions"], list):
        raise PackageError("document.extensions must be an array")

    refs: list[tuple[str, str]] = []
    ids: set[str] = set()
    paths: set[str] = set()
    for index, ref in enumerate(document["levels"]):
        label = f"document.levels[{index}]"
        if not isinstance(ref, dict):
            raise PackageError(f"{label}: expected object")
        if "id" not in ref or "path" not in ref:
            raise PackageError(f"{label}: id and path are required")
        level_id = canonical_uuid(ref["id"], f"{label}.id")
        level_path = safe_package_path(ref["path"])
        if level_id in ids:
            raise PackageError(f"{label}: duplicate level UUID")
        if level_path in paths:
            raise PackageError(f"{label}: duplicate level path")
        if not level_path.startswith("levels/") or not level_path.endswith(".json"):
            raise PackageError(f"{label}: level path must be levels/<uuid>.json")
        ids.add(level_id)
        paths.add(level_path)
        refs.append((level_id, level_path))
    return refs


def _validate_level(level: Any, expected_id: str, label: str) -> None:
    if not isinstance(level, dict):
        raise PackageError(f"{label}: root must be an object")
    required = (
        "$schema",
        "id",
        "name",
        "metadata",
        "geometry",
        "surfaces",
        "world",
        "terminals",
        "editor",
        "extensions",
        "provenance",
    )
    for key in required:
        if key not in level:
            raise PackageError(f"{label}: missing required field {key!r}")
    if level["$schema"] != LEVEL_SCHEMA:
        raise PackageError(f"{label}: unsupported schema identifier")
    level_id = canonical_uuid(level["id"], f"{label}.id")
    if level_id != expected_id:
        raise PackageError(f"{label}: UUID disagrees with document roster")
    if not isinstance(level["name"], str):
        raise PackageError(f"{label}.name must be a string")
    for section in ("metadata", "geometry", "surfaces", "world", "terminals",
                    "editor", "extensions", "provenance"):
        if not isinstance(level[section], dict):
            raise PackageError(f"{label}.{section} must be an object")

    # FORMAT-1A section-level reference hygiene.  FORMAT-1B will make each
    # semantic entity schema strict after the complete field audit.
    entity_arrays: list[tuple[str, Any]] = []
    for key in ("points", "lines", "polygons"):
        entity_arrays.append((f"{label}.geometry.{key}", level["geometry"].get(key, [])))
    entity_arrays.append((f"{label}.surfaces.sides", level["surfaces"].get("sides", [])))
    for key in ("lights", "media", "platforms", "objects", "itemPlacements",
                "ambientSounds", "randomSounds"):
        entity_arrays.append((f"{label}.world.{key}", level["world"].get(key, [])))

    all_ids: set[str] = set()
    for array_label, array in entity_arrays:
        if not isinstance(array, list):
            raise PackageError(f"{array_label} must be an array")
        for index, entity in enumerate(array):
            item_label = f"{array_label}[{index}]"
            if not isinstance(entity, dict) or "id" not in entity:
                raise PackageError(f"{item_label}: entity object with id is required")
            entity_id = canonical_uuid(entity["id"], f"{item_label}.id")
            if entity_id in all_ids:
                raise PackageError(f"{item_label}: duplicate entity UUID {entity_id}")
            all_ids.add(entity_id)


def validate_package(path: Path) -> dict[str, Any]:
    _, entries = _read_zip_entries(path)

    for required in ("manifest.json", "document.json"):
        if required not in entries:
            raise PackageError(f"missing required package resource {required!r}")

    manifest = load_json_bytes(entries["manifest.json"], "manifest.json")
    _validate_manifest(manifest)

    resource_paths: set[str] = set()
    resources: dict[str, dict[str, Any]] = {}
    for index, record in enumerate(manifest["resources"]):
        label = f"manifest.resources[{index}]"
        if not isinstance(record, dict):
            raise PackageError(f"{label}: expected object")
        for key in ("path", "mediaType", "sha256"):
            if key not in record:
                raise PackageError(f"{label}: missing {key!r}")
        package_path = safe_package_path(record["path"])
        if package_path in ("mimetype", "manifest.json"):
            raise PackageError(f"{label}: {package_path!r} is not hash-listed")
        if package_path in resource_paths:
            raise PackageError(f"{label}: duplicate resource path {package_path!r}")
        if package_path not in entries:
            raise PackageError(f"{label}: resource does not exist: {package_path!r}")
        digest = _sha256(entries[package_path])
        if record["sha256"] != digest:
            raise PackageError(
                f"{label}: SHA-256 mismatch for {package_path!r}: "
                f"manifest={record['sha256']} actual={digest}"
            )
        resource_paths.add(package_path)
        resources[package_path] = record

    actual_normative = set(entries) - {"mimetype", "manifest.json"}
    if resource_paths != actual_normative:
        missing = sorted(actual_normative - resource_paths)
        extra = sorted(resource_paths - actual_normative)
        pieces = []
        if missing:
            pieces.append("unlisted entries: " + ", ".join(missing))
        if extra:
            pieces.append("missing entries: " + ", ".join(extra))
        raise PackageError("manifest resource inventory mismatch: " + "; ".join(pieces))

    document_path = safe_package_path(manifest["document"])
    if document_path not in entries:
        raise PackageError(f"manifest document resource is missing: {document_path!r}")
    document = load_json_bytes(entries[document_path], document_path)
    level_refs = _validate_document(document, manifest)

    for level_id, level_path in level_refs:
        if level_path not in entries:
            raise PackageError(f"document references missing level resource {level_path!r}")
        if resources[level_path].get("mediaType") != "application/json":
            raise PackageError(f"{level_path!r} must be declared application/json")
        level = load_json_bytes(entries[level_path], level_path)
        _validate_level(level, level_id, level_path)

    return {
        "path": str(path),
        "kind": manifest["kind"],
        "format": manifest["format"],
        "formatVersion": manifest["formatVersion"],
        "documentId": document["id"],
        "levelCount": len(level_refs),
        "resources": len(resource_paths),
        "extensions": len(manifest["extensions"]),
    }


def identify(path: Path) -> str:
    if path.is_dir():
        if (path / "mimetype").is_file():
            try:
                if (path / "mimetype").read_bytes() == MIMETYPE:
                    return "pfhorge-native-unpacked"
            except OSError:
                pass
        return "directory"

    try:
        head = path.read_bytes()[:64]
    except OSError as exc:
        raise PackageError(f"cannot read {path}: {exc}") from exc

    if head.startswith(b"PK\x03\x04"):
        try:
            validate_package(path)
            return "pfhorge-native-vnext"
        except PackageError:
            return "zip-or-invalid-pfhorge-package"

    # Legacy .pfhlev signature after the two-byte version field.
    # Values come from the existing PhPfhorgeSingleLevelDoc / LEMapData code:
    # sig1=26743, sig2=34521, sig3=42296737, all stored big-endian.
    if len(head) >= 10:
        sig1, sig2, sig3 = struct.unpack(">HHI", head[2:10])
        if (sig1, sig2, sig3) == (26743, 34521, 42296737):
            return "pfhorge-legacy-pfhlev"

    return "unknown"


def read_unpacked_directory(directory: Path) -> dict[str, bytes]:
    if not directory.is_dir():
        raise PackageError(f"{directory}: not a directory")

    entries: dict[str, bytes] = {}
    for fs_path in sorted(directory.rglob("*")):
        if fs_path.is_symlink():
            raise PackageError(f"symbolic links are forbidden: {fs_path}")
        if fs_path.is_dir():
            continue
        rel = fs_path.relative_to(directory).as_posix()
        safe_package_path(rel)
        try:
            data = fs_path.read_bytes()
        except OSError as exc:
            raise PackageError(f"cannot read {fs_path}: {exc}") from exc
        if len(data) > MAX_ENTRY_BYTES:
            raise PackageError(f"{rel!r} exceeds the per-entry safety limit")
        entries[rel] = data

    if len(entries) > MAX_ENTRIES:
        raise PackageError("unpacked package exceeds entry-count safety limit")
    if sum(map(len, entries.values())) > MAX_TOTAL_BYTES:
        raise PackageError("unpacked package exceeds total-size safety limit")
    if entries.get("mimetype") != MIMETYPE:
        raise PackageError("unpacked package has missing/invalid mimetype")
    return entries


def refresh_manifest(entries: dict[str, bytes], *, kind: str | None = None) -> dict[str, bytes]:
    if "manifest.json" not in entries or "document.json" not in entries:
        raise PackageError("manifest.json and document.json are required")
    manifest = load_json_bytes(entries["manifest.json"], "manifest.json")
    _validate_manifest(manifest, require_resources=False)
    if kind is not None:
        if kind not in ("level", "scenario"):
            raise PackageError("kind must be level or scenario")
        manifest["kind"] = kind

    resources = []
    for name in sorted(entries):
        if name in ("mimetype", "manifest.json"):
            continue
        media_type = "application/json" if name.endswith(".json") else "application/octet-stream"
        resources.append({
            "mediaType": media_type,
            "path": name,
            "sha256": _sha256(entries[name]),
        })
    manifest["resources"] = resources
    entries = dict(entries)
    entries["manifest.json"] = dump_json_bytes(manifest)
    return entries


def write_zip(
    entries: dict[str, bytes],
    output: Path,
    *,
    compression_name: str = "store",
) -> None:
    if entries.get("mimetype") != MIMETYPE:
        raise PackageError("mimetype is missing or invalid")
    if compression_name == "store":
        default_method = zipfile.ZIP_STORED
    elif compression_name == "deflate":
        default_method = zipfile.ZIP_DEFLATED
    else:
        raise PackageError("compression must be 'store' or 'deflate'")

    output.parent.mkdir(parents=True, exist_ok=True)
    temp = output.with_name(f".{output.name}.{uuid.uuid4()}.tmp")
    try:
        with zipfile.ZipFile(temp, "w", allowZip64=False) as zf:
            # Required first uncompressed magic entry.
            zf.writestr(
                _zip_info("mimetype", compression=zipfile.ZIP_STORED),
                MIMETYPE,
            )
            for name in sorted(n for n in entries if n != "mimetype"):
                safe_package_path(name)
                method = default_method
                zf.writestr(_zip_info(name, compression=method), entries[name])
        # Validate the complete staged output before atomic replacement.
        validate_package(temp)
        os.replace(temp, output)
    except Exception:
        try:
            temp.unlink(missing_ok=True)
        except OSError:
            pass
        raise


def unpack(package: Path, destination: Path) -> None:
    _, entries = _read_zip_entries(package)
    validate_package(package)
    if destination.exists():
        if any(destination.iterdir()) if destination.is_dir() else True:
            raise PackageError(f"destination already exists and is not empty: {destination}")
    destination.mkdir(parents=True, exist_ok=True)

    root = destination.resolve()
    for name, data in entries.items():
        safe_package_path(name)
        target = (destination / Path(*PurePosixPath(name).parts)).resolve()
        if root != target and root not in target.parents:
            raise PackageError(f"refusing extraction outside destination: {name!r}")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)


def pack(directory: Path, output: Path, *, kind: str | None, compression: str) -> None:
    entries = read_unpacked_directory(directory)
    # Validate the JSON before rebuilding hashes.
    for name, data in entries.items():
        if name.endswith(".json"):
            load_json_bytes(data, name)
    entries = refresh_manifest(entries, kind=kind)
    write_zip(entries, output, compression_name=compression)


def create_sample(output: Path, *, kind: str) -> None:
    if kind not in ("level", "scenario"):
        raise PackageError("sample kind must be level or scenario")

    doc_id = str(uuid.uuid4())
    level_count = 1 if kind == "level" else 2
    levels = []
    entries: dict[str, bytes] = {"mimetype": MIMETYPE}

    for index in range(level_count):
        level_id = str(uuid.uuid4())
        level_path = f"levels/{level_id}.json"
        level_name = "Example Level" if level_count == 1 else f"Example Level {index + 1}"
        level = {
            "$schema": LEVEL_SCHEMA,
            "editor": {"layers": []},
            "extensions": {},
            "geometry": {
                "lines": [],
                "points": [],
                "polygons": [],
            },
            "id": level_id,
            "metadata": {
                "environment": None,
                "entryPointFlags": None,
                "environmentFlags": None,
                "missionFlags": None,
                "physicsModel": None,
                "songIndex": None,
            },
            "name": level_name,
            "provenance": {},
            "surfaces": {"sides": []},
            "terminals": {},
            "world": {
                "ambientSounds": [],
                "itemPlacements": [],
                "lights": [],
                "media": [],
                "objects": [],
                "platforms": [],
                "randomSounds": [],
            },
        }
        entries[level_path] = dump_json_bytes(level)
        levels.append({"id": level_id, "name": level_name, "path": level_path})

    document = {
        "$schema": DOCUMENT_SCHEMA,
        "extensions": [],
        "id": doc_id,
        "kind": kind,
        "levels": levels,
        "metadata": {},
        "title": "Pfhorge Native FORMAT-1A Example",
    }
    entries["document.json"] = dump_json_bytes(document)

    manifest = {
        "$schema": MANIFEST_SCHEMA,
        "document": "document.json",
        "extensions": [],
        "format": FORMAT_ID,
        "formatVersion": FORMAT_VERSION,
        "kind": kind,
        "resources": [],
    }
    entries["manifest.json"] = dump_json_bytes(manifest)
    entries = refresh_manifest(entries)
    write_zip(entries, output, compression_name="store")


def inspect_package(path: Path) -> dict[str, Any]:
    info = validate_package(path)
    _, entries = _read_zip_entries(path)
    manifest = load_json_bytes(entries["manifest.json"], "manifest.json")
    document = load_json_bytes(entries[manifest["document"]], manifest["document"])
    return {
        **info,
        "mimetype": MIMETYPE.decode("ascii"),
        "title": document.get("title", ""),
        "levels": document["levels"],
        "extensionDeclarations": manifest["extensions"],
        "resourceInventory": manifest["resources"],
    }


def _cmd_identify(args: argparse.Namespace) -> None:
    print(identify(Path(args.path)))


def _cmd_validate(args: argparse.Namespace) -> None:
    result = validate_package(Path(args.path))
    print(
        f"VALID: Pfhorge Native {result['formatVersion']} "
        f"{result['kind']} package; levels={result['levelCount']} "
        f"resources={result['resources']} extensions={result['extensions']}"
    )


def _cmd_inspect(args: argparse.Namespace) -> None:
    print(json.dumps(inspect_package(Path(args.path)), indent=2, sort_keys=True))


def _cmd_unpack(args: argparse.Namespace) -> None:
    unpack(Path(args.package), Path(args.destination))
    print(f"Unpacked {args.package} -> {args.destination}")


def _cmd_pack(args: argparse.Namespace) -> None:
    pack(
        Path(args.directory),
        Path(args.package),
        kind=args.kind,
        compression=args.compression,
    )
    print(f"Packed {args.directory} -> {args.package}")


def _cmd_sample(args: argparse.Namespace) -> None:
    create_sample(Path(args.package), kind=args.kind)
    print(f"Created {args.kind} sample: {args.package}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Pfhorge Native vNext FORMAT-1A reference package tool"
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("identify", help="identify legacy/vNext Pfhorge packaging")
    p.add_argument("path")
    p.set_defaults(func=_cmd_identify)

    p = sub.add_parser("validate", help="validate a packed vNext package")
    p.add_argument("path")
    p.set_defaults(func=_cmd_validate)

    p = sub.add_parser("inspect", help="print package manifest/document summary")
    p.add_argument("path")
    p.set_defaults(func=_cmd_inspect)

    p = sub.add_parser("unpack", help="safely unpack a vNext package")
    p.add_argument("package")
    p.add_argument("destination")
    p.set_defaults(func=_cmd_unpack)

    p = sub.add_parser("pack", help="pack an unpacked Pfhorge directory")
    p.add_argument("directory")
    p.add_argument("package")
    p.add_argument("--kind", choices=("level", "scenario"))
    p.add_argument(
        "--compression",
        choices=("store", "deflate"),
        default="store",
        help="writer preference; readers support both (default: store)",
    )
    p.set_defaults(func=_cmd_pack)

    p = sub.add_parser("create-sample", help="create a minimal valid vNext package")
    p.add_argument("package")
    p.add_argument("--kind", choices=("level", "scenario"), default="level")
    p.set_defaults(func=_cmd_sample)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        args.func(args)
    except PackageError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    except (OSError, zipfile.BadZipFile) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
