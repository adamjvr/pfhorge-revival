#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

MIME = b"application/vnd.pfhorge.package+zip"
CANON_EXT = "org.pfhorge.format3b.canonical-authority"

P0 = "00000000-0000-4000-8000-000000000001"
P1 = "00000000-0000-4000-8000-000000000002"
P2 = "00000000-0000-4000-8000-000000000003"
L0 = "00000000-0000-4000-8000-000000000011"
L1 = "00000000-0000-4000-8000-000000000012"
L2 = "00000000-0000-4000-8000-000000000013"
POLY = "00000000-0000-4000-8000-000000000021"
S0 = "00000000-0000-4000-8000-000000000031"
S1 = "00000000-0000-4000-8000-000000000032"
S2 = "00000000-0000-4000-8000-000000000033"
DOC = "00000000-0000-4000-8000-000000000041"


def surface():
    return {"texture": None, "offset": {"x": 0, "y": 0}, "transferMode": 0, "light": None}


def side(ident: str, line: str):
    return {
        "id": ident, "line": line, "polygon": POLY, "type": "full", "flags": 0,
        "primary": surface(), "secondary": None, "transparent": None,
        "controlPanel": None, "ambientDelta": 0,
    }


def level_fixture() -> dict:
    counts = {
        "points": 3, "lines": 3, "polygons": 1, "sides": 3,
        "lights": 0, "media": 0, "platforms": 0, "objects": 0,
        "itemPlacements": 0, "ambientSounds": 0, "randomSounds": 0,
        "tags": 0, "annotations": 0, "layers": 0, "noteGroups": 0,
        "terminals": 0,
    }
    plane = {"height": 0, "texture": None, "transferMode": 0,
             "origin": {"x": 0, "y": 0}, "light": None}
    ceiling = dict(plane)
    ceiling["height"] = 1024
    return {
        "$schema": "urn:pfhorge:schema:level:1",
        "id": POLY,
        "name": "FORMAT-3B Triangle",
        "metadata": {
            "environment": {"classicCode": 1}, "physicsModel": 1, "songIndex": 0,
            "missionFlags": 0, "environmentFlags": 0, "entryPointFlags": 0,
        },
        "geometry": {
            "points": [
                {"id": P0, "x": 0, "y": 0},
                {"id": P1, "x": 1024, "y": 0},
                {"id": P2, "x": 0, "y": 1024},
            ],
            "lines": [
                {"id": L0, "startPoint": P0, "endPoint": P1, "flags": 0, "clockwisePolygon": POLY, "counterclockwisePolygon": None, "clockwiseSide": S0, "counterclockwiseSide": None},
                {"id": L1, "startPoint": P1, "endPoint": P2, "flags": 0, "clockwisePolygon": POLY, "counterclockwisePolygon": None, "clockwiseSide": S1, "counterclockwiseSide": None},
                {"id": L2, "startPoint": P2, "endPoint": P0, "flags": 0, "clockwisePolygon": POLY, "counterclockwisePolygon": None, "clockwiseSide": S2, "counterclockwiseSide": None},
            ],
            "polygons": [{
                "id": POLY, "type": 0, "flags": 0,
                "permutation": {"kind": "integer", "value": 0},
                "edges": [
                    {"line": L0, "side": S0, "direction": "forward"},
                    {"line": L1, "side": S1, "direction": "forward"},
                    {"line": L2, "side": S2, "direction": "forward"},
                ],
                "floor": plane, "ceiling": ceiling,
                "media": None, "ambientSound": None, "randomSound": None,
            }],
        },
        "surfaces": {"sides": [side(S0, L0), side(S1, L1), side(S2, L2)]},
        "world": {
            "lights": [], "media": [], "platforms": [], "objects": [],
            "itemPlacements": [], "ambientSounds": [], "randomSounds": [],
            "tags": [], "annotations": [],
        },
        "terminals": {"items": []},
        "editor": {
            "layers": [], "noteGroups": [], "names": {}, "lineOverrides": [],
            "currentLayer": None, "levelOptions": {},
        },
        "extensions": {
            CANON_EXT: {
                "canonicalAuthority": True, "canonicalModelRevision": 3,
                "legacyBridgeRequired": False, "objectCounts": counts,
                "cocoaFidelity": {},
            }
        },
        "provenance": {"sources": [], "bindings": [], "opaqueFragments": []},
    }


def make_package(path: Path, level: dict) -> None:
    level_path = f"levels/{level['id']}.json"
    level_data = json.dumps(level, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()
    document = {
        "$schema": "urn:pfhorge:schema:document:1", "id": DOC, "kind": "level",
        "title": level["name"],
        "levels": [{"id": level["id"], "path": level_path, "name": level["name"]}],
        "extensions": [CANON_EXT], "metadata": {"writer": "FORMAT-3B test"},
    }
    document_data = json.dumps(document, sort_keys=True, separators=(",", ":")).encode()
    manifest = {
        "$schema": "urn:pfhorge:schema:manifest:1", "format": "org.pfhorge.native",
        "formatVersion": "1.1.0-draft.1", "kind": "level", "document": "document.json",
        "extensions": [{"id": CANON_EXT, "version": "1.0", "requiredForRead": False,
                        "requiredForWrite": False, "resources": []}],
        "resources": [
            {"path": "document.json", "mediaType": "application/json", "role": "document",
             "sha256": hashlib.sha256(document_data).hexdigest()},
            {"path": level_path, "mediaType": "application/json", "role": "canonical-level",
             "sha256": hashlib.sha256(level_data).hexdigest()},
        ],
    }
    manifest_data = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as zf:
        zf.writestr("mimetype", MIME)
        zf.writestr("manifest.json", manifest_data)
        zf.writestr("document.json", document_data)
        zf.writestr(level_path, level_data)



def validate_json_schema_if_available(root: Path, level: dict) -> None:
    schema_dir = root / "schemas/pfhorge-native"
    level_schema_path = schema_dir / "level.schema.json"
    if not level_schema_path.is_file():
        return
    try:
        import jsonschema
        from referencing import Registry, Resource
    except Exception:
        return
    registry = Registry()
    for schema_path in sorted(schema_dir.glob("*.schema.json")):
        contents = json.loads(schema_path.read_text(encoding="utf-8"))
        resource = Resource.from_contents(contents)
        registry = registry.with_resource(schema_path.resolve().as_uri(), resource)
        if contents.get("$id"):
            registry = registry.with_resource(contents["$id"], resource)
    schema = json.loads(level_schema_path.read_text(encoding="utf-8"))
    errors = sorted(
        jsonschema.Draft202012Validator(schema, registry=registry).iter_errors(level),
        key=lambda error: list(error.path),
    )
    if errors:
        details = "\n".join(f"{list(e.path)}: {e.message}" for e in errors[:20])
        raise AssertionError("FORMAT-3B fixture violates FORMAT-1C JSON Schema:\n" + details)

def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    canonical = load_module(root / "scripts/revival/pfhorge_canonical.py", "pfhorge_canonical_test")
    validator = root / "scripts/revival/validate_format3b_package.py"
    validator_module = load_module(validator, "format3b_validator_test")

    level = level_fixture()
    validate_json_schema_if_available(root, level)
    canonical.validate_level(level)
    validator_module._validate_directed_polygon_loops(level)

    # Negative graph check: missing FORMAT-3B line ownership must be rejected.
    broken = json.loads(json.dumps(level))
    del broken["geometry"]["lines"][0]["clockwisePolygon"]
    try:
        canonical.validate_level(broken)
    except canonical.CanonicalError:
        pass
    else:
        raise AssertionError("FORMAT-3B canonical validator accepted a line missing ownership fields")

    # Negative graph check: polygon must agree with exactly one persisted line owner.
    broken = json.loads(json.dumps(level))
    broken["geometry"]["lines"][0]["clockwisePolygon"] = None
    try:
        canonical.validate_level(broken)
    except canonical.CanonicalError:
        pass
    else:
        raise AssertionError("FORMAT-3B canonical validator accepted a polygon/line owner mismatch")

    # Negative graph check: a duplicate stable UUID must be rejected.
    broken = json.loads(json.dumps(level))
    broken["geometry"]["points"][1]["id"] = P0
    try:
        canonical.validate_level(broken)
    except canonical.CanonicalError:
        pass
    else:
        raise AssertionError("duplicate canonical UUID was not rejected")

    broken_loop = json.loads(json.dumps(level))
    broken_loop["geometry"]["polygons"][0]["edges"][1]["direction"] = "reverse"
    try:
        validator_module._validate_directed_polygon_loops(broken_loop)
    except SystemExit:
        pass
    else:
        raise AssertionError("non-continuous canonical polygon loop was not rejected")

    with tempfile.TemporaryDirectory(prefix="pfhorge-format3b-test-") as temp:
        package = Path(temp) / "triangle.pfhlev"
        make_package(package, level)
        subprocess.run([sys.executable, str(validator), str(package), "--repo-root", str(root)], check=True)
        with zipfile.ZipFile(package) as zf:
            assert zf.namelist()[0] == "mimetype"
            assert "bridge/level.archive" not in zf.namelist()

    print("FORMAT-3B canonical-authority tests: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
