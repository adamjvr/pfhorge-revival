#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

echo "=== Pfhorge Native FORMAT-1A ==="
echo "[1/3] Python syntax"
python3 -m py_compile scripts/revival/pfhorge_native.py \
    scripts/revival/tests/test_pfhorge_native.py

echo "[2/3] Package/security smoke tests"
python3 scripts/revival/tests/test_pfhorge_native.py

echo "[3/3] Reference package create/validate/inspect"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/pfhorge-native-check.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

python3 scripts/revival/pfhorge_native.py create-sample "$TMP/example.pfhlev"
python3 scripts/revival/pfhorge_native.py validate "$TMP/example.pfhlev"
python3 scripts/revival/pfhorge_native.py unpack "$TMP/example.pfhlev" "$TMP/unpacked"
python3 scripts/revival/pfhorge_native.py pack \
    "$TMP/unpacked" "$TMP/repacked.pfhlev" --compression deflate
python3 scripts/revival/pfhorge_native.py validate "$TMP/repacked.pfhlev"

echo
echo "FORMAT-1A reference checks passed."
