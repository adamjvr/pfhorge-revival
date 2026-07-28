#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

"""Convert an xcodebuild transcript into compact JSON and Markdown reports."""

from __future__ import annotations

import argparse
import collections
import json
import re
from pathlib import Path

ERROR_RE = re.compile(r"(?:^|\s)(?:fatal )?error:\s*(.+)$", re.IGNORECASE)
WARNING_RE = re.compile(r"(?:^|\s)warning:\s*(.+)$", re.IGNORECASE)
FILE_DIAGNOSTIC_RE = re.compile(
    r"^(?P<path>.+?):(?P<line>\d+)(?::(?P<column>\d+))?:\s*"
    r"(?P<kind>fatal error|error|warning):\s*(?P<message>.+)$",
    re.IGNORECASE,
)


def normalize(text: str) -> str:
    return " ".join(text.strip().split())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--status", type=int, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace") if args.log.exists() else ""
    diagnostics: list[dict[str, object]] = []
    unique = set()

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        match = FILE_DIAGNOSTIC_RE.match(line)
        if match:
            item = {
                "kind": match.group("kind").lower(),
                "path": match.group("path"),
                "line": int(match.group("line")),
                "column": int(match.group("column")) if match.group("column") else None,
                "message": normalize(match.group("message")),
            }
        else:
            error = ERROR_RE.search(line)
            warning = WARNING_RE.search(line)
            if error:
                item = {"kind": "error", "path": None, "line": None, "column": None, "message": normalize(error.group(1))}
            elif warning:
                item = {"kind": "warning", "path": None, "line": None, "column": None, "message": normalize(warning.group(1))}
            else:
                continue

        key = (item["kind"], item["path"], item["line"], item["message"])
        if key not in unique:
            unique.add(key)
            diagnostics.append(item)

    counts = collections.Counter("error" if "error" in str(d["kind"]) else "warning" for d in diagnostics)
    result = {
        "xcodebuild_exit_status": args.status,
        "succeeded": args.status == 0,
        "diagnostic_counts": dict(counts),
        "diagnostics": diagnostics,
    }

    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "build-diagnostics.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Pfhorge Baseline Build",
        "",
        f"- Result: **{'SUCCESS' if args.status == 0 else 'FAILED'}**",
        f"- `xcodebuild` exit status: `{args.status}`",
        f"- Unique errors: **{counts.get('error', 0)}**",
        f"- Unique warnings: **{counts.get('warning', 0)}**",
        "",
        "## Diagnostics",
        "",
    ]
    if not diagnostics:
        lines.append("No compiler-style diagnostics were detected in the transcript.")
    for item in diagnostics[:250]:
        location = item["path"] or "xcodebuild"
        if item["line"]:
            location += f":{item['line']}"
            if item["column"]:
                location += f":{item['column']}"
        lines.append(f"- **{str(item['kind']).upper()}** `{location}` — {item['message']}")
    if len(diagnostics) > 250:
        lines.append(f"- … {len(diagnostics) - 250} additional diagnostics are in `build-diagnostics.json`.")

    (args.output_dir / "build-report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
