#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import zipfile

MIME = b"application/vnd.pfhorge.package+zip"

def classify(path: Path) -> tuple[str, str]:
    data = path.read_bytes()

    if data.startswith(b"PK\x03\x04"):
        try:
            with zipfile.ZipFile(path) as zf:
                names = zf.namelist()
                if (
                    names
                    and names[0] == "mimetype"
                    and zf.read("mimetype") == MIME
                ):
                    return "native", "Pfhorge Native ZIP"
                return "unknown", "ZIP, but not Pfhorge Native"
        except Exception as exc:
            return "unknown", f"invalid ZIP: {exc}"

    if len(data) >= 10:
        sig1, sig2 = struct.unpack(">HH", data[2:6])
        sig3 = struct.unpack(">I", data[6:10])[0]
        if (sig1, sig2, sig3) == (
            26743,
            34521,
            42296737,
        ):
            version = struct.unpack(">H", data[:2])[0]
            return (
                "legacy",
                f"legacy Pfhorge header version {version}",
            )

    return "unknown", "unrecognized bytes"

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--allow-legacy", action="store_true")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    root = Path(args.path).expanduser()

    if root.is_file():
        files = [root]
    else:
        files = sorted(root.rglob("*.pfhlev"))

    rows = []
    for path in files:
        kind, note = classify(path)
        rows.append(
            {
                "path": str(path),
                "kind": kind,
                "note": note,
            }
        )

    counts = {
        "native": sum(
            row["kind"] == "native"
            for row in rows
        ),
        "legacy": sum(
            row["kind"] == "legacy"
            for row in rows
        ),
        "unknown": sum(
            row["kind"] == "unknown"
            for row in rows
        ),
    }

    if args.json:
        print(
            json.dumps(
                {
                    "counts": counts,
                    "files": rows,
                },
                indent=2,
            )
        )
    else:
        print("=== PFHORGE LEVEL TREE AUDIT ===")
        for row in rows:
            print(
                f"{row['kind'].upper():7} "
                f"{row['path']}"
            )
            print(f"        {row['note']}")
        print()
        print(
            f"native={counts['native']} "
            f"legacy={counts['legacy']} "
            f"unknown={counts['unknown']}"
        )

    if counts["unknown"]:
        return 2
    if counts["legacy"] and not args.allow_legacy:
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
