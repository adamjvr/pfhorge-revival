#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

"""Static source and Xcode-project audit for the Pfhorge revival.

The script is intentionally dependency-free so it works on a clean macOS,
Linux, or GitHub Actions runner. It never rewrites source files.
"""

from __future__ import annotations

import argparse
import collections
import datetime as dt
import json
import os
import re
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

SOURCE_EXTENSIONS = {
    ".c": "C",
    ".h": "Header",
    ".m": "Objective-C",
    ".mm": "Objective-C++",
    ".swift": "Swift",
    ".cpp": "C++",
    ".cc": "C++",
    ".xib": "Interface Builder",
    ".storyboard": "Interface Builder",
    ".plist": "Property List",
    ".strings": "Localization",
    ".xcstrings": "String Catalog",
    ".sdef": "AppleScript Dictionary",
}

SKIP_DIRS = {
    ".git",
    ".build",
    "build",
    "DerivedData",
    "RevivalArtifacts",
    "xcuserdata",
}

PATTERNS = {
    "opengl": re.compile(r"\b(?:NSOpenGL|OpenGL|GLKit|gl[A-Z][A-Za-z0-9_]*)\b"),
    "deprecated_appkit": re.compile(r"\b(?:NSDrawer|NSMatrix|NSForm)\b"),
    "carbon_quickdraw": re.compile(r"\b(?:Carbon|QuickDraw|GrafPtr|GWorld|PicHandle|PICT)\b"),
    "manual_memory": re.compile(r"\b(?:retain|release|autorelease|dealloc)\b"),
    "legacy_archiving": re.compile(r"\b(?:NSArchiver|NSUnarchiver|NSKeyedArchiver|NSKeyedUnarchiver)\b"),
    "byte_swapping": re.compile(r"\b(?:CFSwap|OSSwap|NSSwap|Endian)\w*\b"),
    "unsafe_c_io": re.compile(r"\b(?:strcpy|strcat|sprintf|gets)\s*\("),
    "todo_markers": re.compile(r"\b(?:TODO|FIXME|HACK|XXX)\b", re.IGNORECASE),
    "apple_event_scripting": re.compile(r"\b(?:NSScript|AppleEvent|NSAppleEvent)\w*\b"),
}

PBX_SETTING_PATTERNS = {
    "deployment_targets": re.compile(r"MACOSX_DEPLOYMENT_TARGET\s*=\s*([^;]+);"),
    "swift_versions": re.compile(r"SWIFT_VERSION\s*=\s*([^;]+);"),
    "marketing_versions": re.compile(r"MARKETING_VERSION\s*=\s*([^;]+);"),
    "project_versions": re.compile(r"CURRENT_PROJECT_VERSION\s*=\s*([^;]+);"),
    "product_names": re.compile(r"PRODUCT_NAME\s*=\s*([^;]+);"),
}


@dataclass(frozen=True)
class Match:
    category: str
    path: str
    line: int
    excerpt: str


def iter_files(root: Path) -> Iterable[Path]:
    for directory, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in SKIP_DIRS]
        base = Path(directory)
        for name in filenames:
            path = base / name
            if path.suffix.lower() in SOURCE_EXTENSIONS or name == "project.pbxproj":
                yield path


def git_value(root: Path, args: list[str]) -> str | None:
    try:
        result = subprocess.run(
            ["git", *args], cwd=root, check=True, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip() or None


def read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"warning: could not read {path}: {exc}", file=sys.stderr)
        return None


def first_line_excerpt(line: str, limit: int = 180) -> str:
    compact = " ".join(line.strip().split())
    return compact if len(compact) <= limit else compact[: limit - 1] + "…"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output-dir", type=Path, default=Path("RevivalArtifacts"))
    args = parser.parse_args()

    root = args.root.resolve()
    output_dir = (root / args.output_dir).resolve() if not args.output_dir.is_absolute() else args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    project = root / "Pfhorge Source" / "Pfhorge.xcodeproj" / "project.pbxproj"
    if not project.is_file():
        parser.error(f"Pfhorge Xcode project not found beneath {root}")

    language_counts: collections.Counter[str] = collections.Counter()
    extension_counts: collections.Counter[str] = collections.Counter()
    line_counts: collections.Counter[str] = collections.Counter()
    matches: list[Match] = []
    files_scanned = 0

    for path in iter_files(root):
        relative = path.relative_to(root).as_posix()
        text = read_text(path)
        if text is None:
            continue
        files_scanned += 1
        extension = path.suffix.lower() or path.name
        language = SOURCE_EXTENSIONS.get(extension, "Xcode Project")
        extension_counts[extension] += 1
        language_counts[language] += 1
        line_counts[language] += text.count("\n") + (1 if text else 0)

        if extension in {".c", ".h", ".m", ".mm", ".swift", ".cpp", ".cc"}:
            for number, line in enumerate(text.splitlines(), start=1):
                for category, regex in PATTERNS.items():
                    if regex.search(line):
                        matches.append(Match(category, relative, number, first_line_excerpt(line)))

    pbx_text = read_text(project) or ""
    settings: dict[str, list[str]] = {}
    for key, regex in PBX_SETTING_PATTERNS.items():
        settings[key] = sorted({value.strip().strip('"') for value in regex.findall(pbx_text)})

    non_arc_files = sorted(set(re.findall(
        r"/\* ([^*]+\.(?:m|mm)) in Sources \*/ = \{[^}]*COMPILER_FLAGS = \"-fno-objc-arc\";",
        pbx_text,
    )))

    category_counts = collections.Counter(match.category for match in matches)
    report = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "root": str(root),
        "git": {
            "commit": git_value(root, ["rev-parse", "HEAD"]),
            "branch": git_value(root, ["branch", "--show-current"]),
            "remote": git_value(root, ["remote", "get-url", "origin"]),
        },
        "files_scanned": files_scanned,
        "language_file_counts": dict(language_counts.most_common()),
        "language_line_counts": dict(line_counts.most_common()),
        "extension_counts": dict(extension_counts.most_common()),
        "xcode_settings": settings,
        "non_arc_source_files": non_arc_files,
        "risk_counts": dict(category_counts.most_common()),
        "matches": [asdict(match) for match in matches],
    }

    json_path = output_dir / "source-audit.json"
    json_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    markdown: list[str] = [
        "# Pfhorge Source Audit",
        "",
        f"Generated: `{report['generated_at_utc']}`",
        "",
        "## Repository",
        "",
        f"- Commit: `{report['git']['commit'] or 'unavailable'}`",
        f"- Branch: `{report['git']['branch'] or 'unavailable'}`",
        f"- Origin: `{report['git']['remote'] or 'unavailable'}`",
        f"- Files scanned: **{files_scanned}**",
        "",
        "## Language inventory",
        "",
        "| Language/resource type | Files | Approximate lines |",
        "|---|---:|---:|",
    ]
    for language, count in language_counts.most_common():
        markdown.append(f"| {language} | {count} | {line_counts[language]} |")

    markdown.extend(["", "## Xcode settings", ""])
    for key, values in settings.items():
        markdown.append(f"- **{key.replace('_', ' ').title()}:** {', '.join(f'`{v}`' for v in values) or 'not found'}")

    markdown.extend([
        "",
        "## Explicit non-ARC source files",
        "",
    ])
    if non_arc_files:
        markdown.extend(f"- `{name}`" for name in non_arc_files)
    else:
        markdown.append("No explicit `-fno-objc-arc` file flags found.")

    markdown.extend([
        "",
        "## Migration-risk indicators",
        "",
        "These are search indicators, not automatically confirmed bugs.",
        "",
        "| Category | Matches |",
        "|---|---:|",
    ])
    for category, count in category_counts.most_common():
        markdown.append(f"| `{category}` | {count} |")

    markdown.extend(["", "## First 200 indicators", ""])
    for match in matches[:200]:
        markdown.append(f"- `{match.path}:{match.line}` **{match.category}:** `{match.excerpt}`")
    if len(matches) > 200:
        markdown.append(f"- … {len(matches) - 200} additional matches are available in `source-audit.json`.")

    md_path = output_dir / "source-audit.md"
    md_path.write_text("\n".join(markdown) + "\n", encoding="utf-8")

    print(f"wrote {md_path}")
    print(f"wrote {json_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
