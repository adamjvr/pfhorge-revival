#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
python3 "$ROOT/scripts/revival/validate_content1a2.py" "$ROOT"
make -f "$ROOT/revival.mk" -C "$ROOT" content1a1-check
