#!/bin/bash
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"

printf 'MAP-1B validation root: %s\n' "$ROOT"

find "$ROOT" -type d -name __pycache__ -prune -print -exec rm -rf {} +
find "$ROOT" -type f -name '*.py[co]' -print -delete

make -C "$ROOT" -f revival.mk map-intake-check
make -C "$ROOT" -f revival.mk preview-core-check
make -C "$ROOT" -f revival.mk baseline

printf '\nMAP-1B compile and baseline validation passed.\n'
printf 'Runtime validation is still required with Detention Center.\n'
