#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
python3 "$ROOT/scripts/revival/validate_tex1a.py" "$ROOT"

OUTPUT="${TMPDIR:-/tmp}/pfhorge-preview-texture-smoke-$$"
PROBE="${TMPDIR:-/tmp}/pfhorge-cxx17-probe-$$.cpp"
PROBE_OUTPUT="${TMPDIR:-/tmp}/pfhorge-cxx17-probe-$$"
trap 'rm -f "$OUTPUT" "$PROBE" "$PROBE_OUTPUT"' EXIT

cat > "$PROBE" <<'CPP'
#include <cstdint>
#include <vector>
int main() {
    std::vector<std::uint32_t> values{1U, 2U, 3U};
    return values.size() == 3U ? 0 : 1;
}
CPP

COMMON_FLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Wpedantic
  -Werror
)

if [[ "$(uname -s)" == "Darwin" ]]; then
  if ! command -v xcrun >/dev/null 2>&1; then
    echo "TEX-1A validation failed: xcrun is unavailable" >&2
    exit 1
  fi

  CANDIDATES=()

  if [[ -n "${DEVELOPER_DIR:-}" && -d "${DEVELOPER_DIR}" ]]; then
    CANDIDATES+=("${DEVELOPER_DIR}")
  fi

  SELECTED_DEVELOPER_DIR="$(xcode-select -p 2>/dev/null || true)"
  if [[ -n "$SELECTED_DEVELOPER_DIR" && -d "$SELECTED_DEVELOPER_DIR" ]]; then
    CANDIDATES+=("$SELECTED_DEVELOPER_DIR")
  fi

  if [[ -d "/Applications/Xcode.app/Contents/Developer" ]]; then
    CANDIDATES+=("/Applications/Xcode.app/Contents/Developer")
  fi

  for candidate in /Applications/Xcode*.app/Contents/Developer; do
    if [[ -d "$candidate" ]]; then
      CANDIDATES+=("$candidate")
    fi
  done

  WORKING_DEVELOPER_DIR=""
  WORKING_SDK=""

  for candidate in "${CANDIDATES[@]}"; do
    sdk_path="$(
      DEVELOPER_DIR="$candidate" \
        xcrun --sdk macosx --show-sdk-path 2>/dev/null || true
    )"
    if [[ -z "$sdk_path" || ! -d "$sdk_path" ]]; then
      continue
    fi

    if DEVELOPER_DIR="$candidate" \
       xcrun --sdk macosx clang++ \
         "${COMMON_FLAGS[@]}" \
         -stdlib=libc++ \
         -isysroot "$sdk_path" \
         "$PROBE" \
         -o "$PROBE_OUTPUT" \
         >/dev/null 2>&1; then
      WORKING_DEVELOPER_DIR="$candidate"
      WORKING_SDK="$sdk_path"
      break
    fi
  done

  if [[ -z "$WORKING_DEVELOPER_DIR" ]]; then
    cat >&2 <<'EOF'
TEX-1A validation failed: no installed Xcode toolchain could compile a
minimal C++17/libc++ program.

Check:
  xcode-select -p
  xcodebuild -version
  xcrun --sdk macosx --show-sdk-path
EOF
    exit 1
  fi

  echo "TEX-1A C++ toolchain: $WORKING_DEVELOPER_DIR"
  echo "TEX-1A macOS SDK: $WORKING_SDK"

  DEVELOPER_DIR="$WORKING_DEVELOPER_DIR" \
    xcrun --sdk macosx clang++ \
      "${COMMON_FLAGS[@]}" \
      -stdlib=libc++ \
      -isysroot "$WORKING_SDK" \
      -I"$ROOT/Pfhorge Source/Preview/Core" \
      "$ROOT/Pfhorge Source/Preview/Tests/PreviewTextureSmoke.cpp" \
      -o "$OUTPUT"
else
  if command -v clang++ >/dev/null 2>&1; then
    CXX="$(command -v clang++)"
  elif command -v g++ >/dev/null 2>&1; then
    CXX="$(command -v g++)"
  else
    echo "TEX-1A validation failed: no C++17 compiler found" >&2
    exit 1
  fi

  "$CXX" \
    "${COMMON_FLAGS[@]}" \
    -I"$ROOT/Pfhorge Source/Preview/Core" \
    "$ROOT/Pfhorge Source/Preview/Tests/PreviewTextureSmoke.cpp" \
    -o "$OUTPUT"
fi

"$OUTPUT"
echo "TEX-1A texture-key smoke test passed"
