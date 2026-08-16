#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
echo "=== Pfhorge FORMAT-1B ==="
echo "[1/4] Python syntax"
python3 -m py_compile scripts/revival/pfhorge_semantic_audit.py scripts/revival/tests/test_pfhorge_semantic_audit.py
echo "[2/4] Semantic-audit unit tests"
python3 scripts/revival/tests/test_pfhorge_semantic_audit.py
echo "[3/4] Canonical C++ foundation"
CXX=""
SDKROOT=""

# Prefer Apple's active macOS SDK explicitly. A bare /usr/bin/clang++ invocation
# can fail to locate libc++ headers when the selected developer directory or
# shell environment does not implicitly provide an SDK sysroot.
if command -v xcrun >/dev/null 2>&1; then
  CXX="$(xcrun --sdk macosx --find clang++ 2>/dev/null || true)"
  SDKROOT="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || true)"
fi

if [[ -z "$CXX" ]]; then
  CXX="$(command -v clang++ || command -v c++ || true)"
fi

[[ -n "$CXX" ]] || { echo "ERROR: no C++ compiler found"; exit 1; }

TMPF="$(mktemp -d "${TMPDIR:-/tmp}/pfhorge-format1b.XXXXXX")"
trap 'rm -rf "$TMPF"; find scripts/revival -type d -name __pycache__ -prune -exec rm -rf {} + 2>/dev/null || true' EXIT

CXXFLAGS=(-std=c++20 -Wall -Wextra -Werror)
if [[ -n "$SDKROOT" ]]; then
  echo "Using macOS SDK: $SDKROOT"
  CXXFLAGS+=(-isysroot "$SDKROOT")
fi

echo "Using C++ compiler: $CXX"
"$CXX" "${CXXFLAGS[@]}" scripts/revival/tests/format1b_core_smoke.cpp -o "$TMPF/core"
"$TMPF/core"
echo "[4/4] Full repository semantic field audit"
python3 scripts/revival/pfhorge_semantic_audit.py --root "$ROOT" --output-dir RevivalArtifacts/FORMAT-1B
echo
echo "FORMAT-1B checks passed."
