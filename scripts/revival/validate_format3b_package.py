#!/usr/bin/env python3
"""Validate a FORMAT-3B canonical-authority .pfhlev package."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import sys
import zipfile
from pathlib import Path

MIME = b"application/vnd.pfhorge.package+zip"
CANON_EXT = "org.pfhorge.format3b.canonical-authority"
BRIDGE = "bridge/level.archive"


def die(message: str) -> "None":
    raise SystemExit(f"ERROR: {message}")


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _load_graph_validator(repo_root: Path):
    helper = repo_root / "scripts/revival/pfhorge_canonical.py"
    if not helper.is_file():
        die(f"canonical graph validator not found: {helper}")
    spec = importlib.util.spec_from_file_location("pfhorge_canonical", helper)
    if spec is None or spec.loader is None:
        die("could not load canonical graph validator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.validate_level



def _validate_directed_polygon_loops(level: dict) -> None:
    lines = {row["id"]: row for row in level["geometry"]["lines"]}
    for polygon in level["geometry"]["polygons"]:
        directed = []
        for edge in polygon["edges"]:
            line = lines.get(edge["line"])
            if line is None:
                die(f"polygon {polygon['id']} references missing line {edge['line']}")
            if edge["direction"] == "forward":
                directed.append((line["startPoint"], line["endPoint"]))
            elif edge["direction"] == "reverse":
                directed.append((line["endPoint"], line["startPoint"]))
            else:
                die(f"polygon {polygon['id']} has invalid edge direction")
        if len(directed) < 3:
            die(f"polygon {polygon['id']} has fewer than three edges")
        for index, (_, end) in enumerate(directed):
            next_start = directed[(index + 1) % len(directed)][0]
            if end != next_start:
                die(f"polygon {polygon['id']} directed edges are not continuous/closed")

def validate_package(path: Path, repo_root: Path) -> dict:
    validate_level = _load_graph_validator(repo_root)

    with zipfile.ZipFile(path) as zf:
        names = zf.namelist()
        if not names or names[0] != "mimetype":
            die("mimetype must be the first ZIP member")
        if zf.read("mimetype") != MIME:
            die("invalid Pfhorge Native mimetype")
        if BRIDGE in names:
            die("FORMAT-3B canonical-authority package contains legacy bridge/level.archive")

        required = {"mimetype", "manifest.json", "document.json"}
        missing = required.difference(names)
        if missing:
            die(f"missing required package member(s): {sorted(missing)}")

        manifest = json.loads(zf.read("manifest.json"))
        if manifest.get("format") != "org.pfhorge.native":
            die("manifest format is not org.pfhorge.native")
        if manifest.get("kind") != "level":
            die("manifest kind is not level")
        if manifest.get("document") != "document.json":
            die("manifest document path is not document.json")

        for resource in manifest.get("resources", []):
            resource_path = resource.get("path")
            expected = resource.get("sha256")
            if not isinstance(resource_path, str) or resource_path not in names:
                die(f"manifest resource missing: {resource_path!r}")
            if not isinstance(expected, str) or _sha256(zf.read(resource_path)) != expected:
                die(f"manifest SHA-256 mismatch: {resource_path}")

        document = json.loads(zf.read(manifest["document"]))
        if document.get("$schema") != "urn:pfhorge:schema:document:1":
            die("unsupported document schema")
        if document.get("kind") != "level":
            die("document kind is not level")
        refs = document.get("levels")
        if not isinstance(refs, list) or len(refs) != 1:
            die("FORMAT-3B level document must contain exactly one level reference")
        level_ref = refs[0]
        level_path = level_ref.get("path")
        if not isinstance(level_path, str) or level_path not in names:
            die("canonical level resource referenced by document is missing")

        level = json.loads(zf.read(level_path))
        if level.get("$schema") != "urn:pfhorge:schema:level:1":
            die("canonical level must use FORMAT-1C level schema urn:pfhorge:schema:level:1")
        if level_ref.get("id") != level.get("id"):
            die("document level UUID does not match canonical level UUID")

        authority = level.get("extensions", {}).get(CANON_EXT)
        if not isinstance(authority, dict):
            die("FORMAT-3B canonical-authority extension is missing")
        if authority.get("canonicalAuthority") is not True:
            die("canonicalAuthority must be true")
        try:
            revision = int(authority.get("canonicalModelRevision", 0))
        except (TypeError, ValueError):
            die("canonicalModelRevision is invalid")
        if revision < 3:
            die("canonicalModelRevision must be >= 3")
        if authority.get("legacyBridgeRequired") is not False:
            die("legacyBridgeRequired must be false")

        # Existing FORMAT-1C validator is the graph-contract source of truth.
        ids = validate_level(level)
        _validate_directed_polygon_loops(level)

        counts = authority.get("objectCounts", {})
        actual = {
            "points": len(level["geometry"]["points"]),
            "lines": len(level["geometry"]["lines"]),
            "polygons": len(level["geometry"]["polygons"]),
            "sides": len(level["surfaces"]["sides"]),
            "lights": len(level["world"]["lights"]),
            "media": len(level["world"]["media"]),
            "platforms": len(level["world"]["platforms"]),
            "objects": len(level["world"]["objects"]),
            "itemPlacements": len(level["world"]["itemPlacements"]),
            "ambientSounds": len(level["world"]["ambientSounds"]),
            "randomSounds": len(level["world"]["randomSounds"]),
            "tags": len(level["world"]["tags"]),
            "annotations": len(level["world"]["annotations"]),
            "terminals": len(level["terminals"]["items"]),
            "layers": len(level["editor"]["layers"]),
            "noteGroups": len(level["editor"]["noteGroups"]),
        }
        for key, value in actual.items():
            if key in counts and counts[key] != value:
                die(f"objectCounts mismatch for {key}: extension={counts[key]} actual={value}")

        return {
            "level": level,
            "revision": revision,
            "counts": actual,
            "entity_count": sum(len(group) for group in ids.values()),
            "members": names,
        }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("file", type=Path)
    ap.add_argument("--repo-root", type=Path)
    args = ap.parse_args()
    repo_root = (args.repo_root or Path(__file__).resolve().parents[2]).resolve()
    result = validate_package(args.file.expanduser().resolve(), repo_root)
    level = result["level"]
    print("FORMAT-3B PACKAGE: PASS")
    print("level:", level["name"], level["id"])
    print("schema:", level["$schema"])
    print("canonicalModelRevision:", result["revision"])
    print("canonicalAuthority: True")
    print("legacyBridgeMember: False")
    print("entities:", result["entity_count"])
    print("counts:")
    for key, value in sorted(result["counts"].items()):
        print(f"  {key:<18} {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
