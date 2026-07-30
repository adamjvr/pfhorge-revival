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

VIS_SOURCE="$ROOT/Pfhorge Source/Preview/Tests/PreviewVisibilitySmoke.cpp"
VIS_OUTPUT="${TMPDIR:-/tmp}/pfhorge-preview-visibility-smoke"

"$CXX" -std=c++17 -Wall -Wextra -Wpedantic -Werror "$VIS_SOURCE" -o "$VIS_OUTPUT"
"$VIS_OUTPUT"
rm -f "$VIS_OUTPUT"


PORTAL_SOURCE="$ROOT/Pfhorge Source/Preview/Tests/PreviewPortalClippingSmoke.cpp"
PORTAL_OUTPUT="${TMPDIR:-/tmp}/pfhorge-preview-portal-clipping-smoke"

"$CXX" -std=c++17 -Wall -Wextra -Wpedantic -Werror "$PORTAL_SOURCE" -o "$PORTAL_OUTPUT"
"$PORTAL_OUTPUT"
rm -f "$PORTAL_OUTPUT"

echo "preview core smoke test passed"

PLAYER_START_SOURCE="$ROOT/Pfhorge Source/Preview/Tests/PreviewPlayerStartSmoke.cpp"
PLAYER_START_OUTPUT="${TMPDIR:-/tmp}/pfhorge-preview-player-start-smoke"

"$CXX" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$PLAYER_START_SOURCE" -o "$PLAYER_START_OUTPUT"
"$PLAYER_START_OUTPUT"
rm -f "$PLAYER_START_OUTPUT"

