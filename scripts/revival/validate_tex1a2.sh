#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
python3 "$ROOT/scripts/revival/validate_tex1a2.py" "$ROOT"

OUTPUT="${TMPDIR:-/tmp}/pfhorge-preview-wall-texture-smoke-$$"
PROBE="${TMPDIR:-/tmp}/pfhorge-tex1a2-cxx17-probe-$$.cpp"
PROBE_OUTPUT="${TMPDIR:-/tmp}/pfhorge-tex1a2-cxx17-probe-$$"
trap 'rm -f "$OUTPUT" "$PROBE" "$PROBE_OUTPUT"' EXIT

cat > "$PROBE" <<'CPP'
#include <cstdint>
#include <vector>
int main() {
    std::vector<std::uint32_t> values{1U, 2U, 3U};
    return values.size() == 3U ? 0 : 1;
}
CPP

FLAGS=(-std=c++17 -Wall -Wextra -Wpedantic -Werror)
if [[ "$(uname -s)" == "Darwin" ]]; then
  CANDIDATES=()
  [[ -n "${DEVELOPER_DIR:-}" && -d "${DEVELOPER_DIR}" ]] &&
    CANDIDATES+=("${DEVELOPER_DIR}")
  SELECTED="$(xcode-select -p 2>/dev/null || true)"
  [[ -n "$SELECTED" && -d "$SELECTED" ]] && CANDIDATES+=("$SELECTED")
  [[ -d /Applications/Xcode.app/Contents/Developer ]] &&
    CANDIDATES+=(/Applications/Xcode.app/Contents/Developer)
  for candidate in /Applications/Xcode*.app/Contents/Developer; do
    [[ -d "$candidate" ]] && CANDIDATES+=("$candidate")
  done

  WORKING=""
  SDK=""
  for candidate in "${CANDIDATES[@]}"; do
    sdk="$(DEVELOPER_DIR="$candidate" \
      xcrun --sdk macosx --show-sdk-path 2>/dev/null || true)"
    [[ -n "$sdk" && -d "$sdk" ]] || continue
    if DEVELOPER_DIR="$candidate" xcrun --sdk macosx clang++ \
        "${FLAGS[@]}" -stdlib=libc++ -isysroot "$sdk" \
        "$PROBE" -o "$PROBE_OUTPUT" >/dev/null 2>&1; then
      WORKING="$candidate"
      SDK="$sdk"
      break
    fi
  done
  if [[ -z "$WORKING" ]]; then
    echo "TEX-1A.2 validation failed: no Xcode C++17/libc++ toolchain works" >&2
    exit 1
  fi
  echo "TEX-1A.2 C++ toolchain: $WORKING"
  echo "TEX-1A.2 macOS SDK: $SDK"
  DEVELOPER_DIR="$WORKING" xcrun --sdk macosx clang++ \
    "${FLAGS[@]}" -stdlib=libc++ -isysroot "$SDK" \
    -I"$ROOT/Pfhorge Source/Preview/Core" \
    "$ROOT/Pfhorge Source/Preview/Tests/PreviewWallTextureSmoke.cpp" \
    -o "$OUTPUT"
else
  CXX="$(command -v clang++ || command -v g++ || true)"
  [[ -n "$CXX" ]] || {
    echo "TEX-1A.2 validation failed: no C++17 compiler" >&2
    exit 1
  }
  "$CXX" "${FLAGS[@]}" \
    -I"$ROOT/Pfhorge Source/Preview/Core" \
    "$ROOT/Pfhorge Source/Preview/Tests/PreviewWallTextureSmoke.cpp" \
    -o "$OUTPUT"
fi

"$OUTPUT"
echo "TEX-1A.2 texture provenance/translucency smoke test passed"

# Preserve every earlier gate. This must not call tex1a2-check again.
"${MAKE:-make}" -C "$ROOT" -f revival.mk vm4a-check
