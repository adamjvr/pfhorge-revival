#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
python3 scripts/revival/validate_content1a1.py --root "$ROOT"
python3 -m py_compile \
  scripts/revival/validate_content1a1.py \
  scripts/revival/content-builders/marathon/build_pack.py \
  scripts/revival/content-builders/marathon2/build_pack.py \
  scripts/revival/content-builders/infinity/build_pack.py
make -f revival.mk content1a-check
printf '%s\n' 'CONTENT-1A.1 validation passed. Run make -f revival.mk baseline on macOS.'
