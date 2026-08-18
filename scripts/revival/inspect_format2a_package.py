#!/usr/bin/env python3
"""Inspect both transitional and canonical-authority Pfhorge Native level packages."""
from __future__ import annotations

import argparse
import json
import zipfile
from pathlib import Path

MIME = b"application/vnd.pfhorge.package+zip"
BRIDGE = "bridge/level.archive"
BRIDGE_EXT = "org.pfhorge.format2a.snapshot-authority"
CANON_EXT = "org.pfhorge.format3b.canonical-authority"


def _counts_from_level(level: dict) -> dict[str, int]:
    geometry = level.get("geometry", {})
    surfaces = level.get("surfaces", {})
    world = level.get("world", {})
    terminals = level.get("terminals", {})
    editor = level.get("editor", {})
    return {
        "points": len(geometry.get("points", [])),
        "lines": len(geometry.get("lines", [])),
        "polygons": len(geometry.get("polygons", [])),
        "sides": len(surfaces.get("sides", [])),
        "lights": len(world.get("lights", [])),
        "media": len(world.get("media", [])),
        "platforms": len(world.get("platforms", [])),
        "objects": len(world.get("objects", [])),
        "itemPlacements": len(world.get("itemPlacements", [])),
        "ambientSounds": len(world.get("ambientSounds", [])),
        "randomSounds": len(world.get("randomSounds", [])),
        "tags": len(world.get("tags", [])),
        "annotations": len(world.get("annotations", [])),
        "terminals": len(terminals.get("items", [])),
        "layers": len(editor.get("layers", [])),
        "noteGroups": len(editor.get("noteGroups", [])),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    args = ap.parse_args()
    path = Path(args.file).expanduser()

    with zipfile.ZipFile(path) as zf:
        names = zf.namelist()
        print("FIRST:", names[0] if names else "<empty>")
        if not names or names[0] != "mimetype" or zf.read("mimetype") != MIME:
            raise SystemExit("ERROR: not a Pfhorge Native package")

        manifest = json.loads(zf.read("manifest.json"))
        document = json.loads(zf.read(manifest["document"]))
        level_ref = document["levels"][0]
        level = json.loads(zf.read(level_ref["path"]))

        extensions = level.get("extensions", {})
        canon = extensions.get(CANON_EXT, {})
        bridge = extensions.get(BRIDGE_EXT, {})
        is_canonical = bool(canon.get("canonicalAuthority"))

        if is_canonical:
            revision = canon.get("canonicalModelRevision", "<missing>")
            authoritative = level_ref["path"]
            counts = canon.get("objectCounts") or _counts_from_level(level)
        else:
            revision = "<transitional>"
            authoritative = bridge.get("authoritativeResource", "<unknown>")
            counts = bridge.get("objectCounts") or _counts_from_level(level)

        print("FORMAT:", manifest.get("format"), manifest.get("formatVersion"))
        print("LEVEL:", level.get("name"), level.get("id"))
        print("SCHEMA:", level.get("$schema", "<missing>"))
        print("CANONICAL MODEL REVISION:", revision)
        print("CANONICAL AUTHORITY:", is_canonical)
        print("AUTHORITATIVE RESOURCE:", authoritative)
        print("LEGACY BRIDGE MEMBER:", BRIDGE in names)
        print("COUNTS:")
        for key, value in sorted(counts.items()):
            print(f"  {key:<18} {value}")
        print("MEMBERS:")
        for name in names:
            print(" ", name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
