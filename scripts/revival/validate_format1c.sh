#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
echo "=== Pfhorge FORMAT-1C ==="
TMPF="$(mktemp -d "${TMPDIR:-/tmp}/pfhorge-format1c.XXXXXX")"
trap 'rm -rf "$TMPF"; find scripts/revival -type d -name __pycache__ -prune -exec rm -rf {} + 2>/dev/null || true' EXIT

echo "[1/6] Python syntax"
python3 -m py_compile scripts/revival/pfhorge_canonical.py scripts/revival/validate_format1c_schema.py scripts/revival/verify_format1c_audit.py scripts/revival/tests/test_pfhorge_canonical.py

echo "[2/6] Canonical semantic tests"
python3 scripts/revival/tests/test_pfhorge_canonical.py

echo "[3/6] JSON Schema set"
python3 scripts/revival/validate_format1c_schema.py

echo "[4/6] Re-run full semantic field audit with FORMAT-1C policy"
python3 scripts/revival/pfhorge_semantic_audit.py --root "$ROOT" --output-dir RevivalArtifacts/FORMAT-1C
python3 scripts/revival/verify_format1c_audit.py RevivalArtifacts/FORMAT-1C/semantic-field-audit.json

echo "[5/6] Canonical C++ level model"
CXX=""; SDKROOT=""
if command -v xcrun >/dev/null 2>&1; then CXX="$(xcrun --sdk macosx --find clang++ 2>/dev/null || true)"; SDKROOT="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || true)"; fi
if [[ -z "$CXX" ]]; then CXX="$(command -v clang++ || command -v c++ || true)"; fi
[[ -n "$CXX" ]] || { echo "ERROR: no C++ compiler found"; exit 1; }
CXXFLAGS=(-std=c++20 -Wall -Wextra -Werror)
if [[ -n "$SDKROOT" ]]; then echo "Using macOS SDK: $SDKROOT"; CXXFLAGS+=(-isysroot "$SDKROOT"); fi
echo "Using C++ compiler: $CXX"
"$CXX" "${CXXFLAGS[@]}" scripts/revival/tests/format1c_core_smoke.cpp -o "$TMPF/core"
"$TMPF/core"

echo "[6/6] FORMAT-1A package regression"
make -f revival.mk native-format-check

echo
echo "FORMAT-1C checks passed."
