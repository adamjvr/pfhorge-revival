#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTIFACTS="$ROOT/RevivalArtifacts/MapIntake"
BUILD_DIR="$ARTIFACTS/build"

mkdir -p "$BUILD_DIR"

clang \
  -std=c11 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -Werror \
  "$ROOT/Pfhorge Source/Map Intake/Tests/MarathonMapProbeSmoke.c" \
  -o "$BUILD_DIR/MarathonMapProbeSmoke"

"$BUILD_DIR/MarathonMapProbeSmoke"

python3 -m py_compile \
  "$ROOT/scripts/revival/probe_map_corpus.py" \
  "$ROOT/Pfhorge Source/Map Intake/Tests/CorpusProbeSmoke.py"

python3 \
  "$ROOT/Pfhorge Source/Map Intake/Tests/CorpusProbeSmoke.py"

if [[ -n "${PFHORGE_MAP_CORPUS:-}" ]]; then
  python3 "$ROOT/scripts/revival/probe_map_corpus.py" \
    "$PFHORGE_MAP_CORPUS" \
    --output-dir "$ARTIFACTS/corpus"
fi

printf '%s\n' 'MAP-1A intake validation passed'
