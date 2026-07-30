#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
python3 "$ROOT/scripts/revival/validate_content1a_runtime_hotfix.py" "$ROOT"
