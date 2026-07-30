#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

python3 scripts/revival/validate_content1a.py --root "$ROOT"
python3 -m py_compile \
  scripts/revival/content_registry_probe.py \
  scripts/revival/validate_content1a.py

# Preserve all previously established gates. The Xcode build is deliberately
# separate so portable CI can run this target on non-macOS hosts.
make -f revival.mk preview-core-check
make -f revival.mk map-intake-check

echo "CONTENT-1A validation passed. Run 'make -f revival.mk baseline' on macOS."
