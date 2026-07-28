#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT="$ROOT/Pfhorge Source/Pfhorge.xcodeproj"
OUTPUT_DIR="${PFHORGE_ARTIFACT_DIR:-$ROOT/RevivalArtifacts}"
DERIVED_DATA="$OUTPUT_DIR/DerivedData"
LOG="$OUTPUT_DIR/xcodebuild.log"
ENV_LOG="$OUTPUT_DIR/build-environment.txt"
LIST_JSON="$OUTPUT_DIR/xcode-project-list.json"
SETTINGS_LOG="$OUTPUT_DIR/build-settings.txt"
ALLOW_FAILURE="${PFHORGE_BASELINE_ALLOW_FAILURE:-0}"
CONFIGURATION="${PFHORGE_CONFIGURATION:-Debug}"

mkdir -p "$OUTPUT_DIR"

if [[ "$(uname -s)" != "Darwin" ]]; then
  cat > "$OUTPUT_DIR/build-report.md" <<'REPORT'
# Pfhorge Baseline Build

The compile step was not run because this host is not macOS. Pfhorge requires Cocoa/AppKit and Xcode.

Run on a Mac:

```bash
make -f revival.mk baseline
```
REPORT
  echo "error: Pfhorge must be built on macOS with Xcode." >&2
  echo "The static source audit remains available with: make -f revival.mk audit" >&2
  exit 2
fi

if ! command -v xcodebuild >/dev/null 2>&1; then
  echo "error: xcodebuild is unavailable. Install Xcode and select it with xcode-select." >&2
  exit 2
fi

if [[ ! -d "$PROJECT" ]]; then
  echo "error: project not found: $PROJECT" >&2
  exit 2
fi

{
  echo "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "host=$(scutil --get ComputerName 2>/dev/null || hostname)"
  echo "architecture=$(uname -m)"
  echo "kernel=$(uname -a)"
  echo "developer_dir=$(xcode-select -p 2>/dev/null || true)"
  echo
  xcodebuild -version
  echo
  swift --version 2>&1 || true
  echo
  clang --version 2>&1 || true
  echo
  sw_vers
} > "$ENV_LOG" 2>&1

xcodebuild -list -json -project "$PROJECT" > "$LIST_JSON" 2> "$OUTPUT_DIR/xcode-project-list.stderr" || true

SCHEME="$(python3 - "$LIST_JSON" <<'PY'
import json, sys
try:
    data = json.load(open(sys.argv[1], encoding='utf-8'))
except Exception:
    print('')
    raise SystemExit
project = data.get('project', {})
schemes = project.get('schemes') or []
for candidate in ('Pfhorge', 'Pfhorge Source'):
    if candidate in schemes:
        print(candidate)
        break
else:
    print(schemes[0] if schemes else '')
PY
)"

TARGET="$(python3 - "$LIST_JSON" <<'PY'
import json, sys
try:
    data = json.load(open(sys.argv[1], encoding='utf-8'))
except Exception:
    print('Pfhorge')
    raise SystemExit
project = data.get('project', {})
targets = project.get('targets') or []
print('Pfhorge' if 'Pfhorge' in targets else (targets[0] if targets else 'Pfhorge'))
PY
)"

SELECTOR=()
BUILD_LOCATION_ARGS=()
if [[ -n "$SCHEME" ]]; then
  SELECTOR=(-scheme "$SCHEME")
  BUILD_LOCATION_ARGS=(-derivedDataPath "$DERIVED_DATA")
else
  SELECTOR=(-target "$TARGET")
  BUILD_LOCATION_ARGS=(SYMROOT="$OUTPUT_DIR/BuildProducts" OBJROOT="$OUTPUT_DIR/BuildIntermediates")
fi

xcodebuild \
  -project "$PROJECT" \
  "${SELECTOR[@]}" \
  -configuration "$CONFIGURATION" \
  -showBuildSettings \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  > "$SETTINGS_LOG" 2>&1 || true

set +e
xcodebuild \
  -project "$PROJECT" \
  "${SELECTOR[@]}" \
  -configuration "$CONFIGURATION" \
  "${BUILD_LOCATION_ARGS[@]}" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  COMPILER_INDEX_STORE_ENABLE=NO \
  clean build 2>&1 | tee "$LOG"
STATUS=${PIPESTATUS[0]}
set -e

python3 "$SCRIPT_DIR/parse_build_log.py" "$LOG" --status "$STATUS" --output-dir "$OUTPUT_DIR"

{
  echo
  echo "## Invocation"
  echo
  printf -- '- Project: `%s`\n' "$PROJECT"
  if [[ -n "$SCHEME" ]]; then
    printf -- '- Scheme: `%s`\n' "$SCHEME"
  else
    printf -- '- Target: `%s`\n' "$TARGET"
  fi
  printf -- '- Configuration: `%s`\n' "$CONFIGURATION"
  printf -- '- Derived data: `%s`\n' "$DERIVED_DATA"
} >> "$OUTPUT_DIR/build-report.md"

if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  cat "$OUTPUT_DIR/build-report.md" >> "$GITHUB_STEP_SUMMARY"
fi

if ((STATUS != 0)); then
  echo "baseline build failed; report: $OUTPUT_DIR/build-report.md" >&2
  if [[ "$ALLOW_FAILURE" == "1" ]]; then
    echo "failure accepted because PFHORGE_BASELINE_ALLOW_FAILURE=1" >&2
    exit 0
  fi
fi

exit "$STATUS"
