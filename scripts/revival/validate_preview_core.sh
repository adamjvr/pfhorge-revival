#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE="$ROOT/Pfhorge Source/Preview/Tests/PreviewCoreSmoke.cpp"
OUTPUT="${TMPDIR:-/tmp}/pfhorge-preview-core-smoke"

if command -v clang++ >/dev/null 2>&1; then
  CXX=clang++
elif command -v c++ >/dev/null 2>&1; then
  CXX=c++
else
  echo "error: no C++ compiler found" >&2
  exit 2
fi

"$CXX" \
  -std=c++20 \
  -Wall -Wextra -Wpedantic -Werror \
  "$SOURCE" \
  -o "$OUTPUT"

"$OUTPUT"
rm -f "$OUTPUT"

echo "preview core smoke test passed"
