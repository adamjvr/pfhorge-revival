#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/1024x1024-master.png" >&2
  exit 2
fi

MASTER="$1"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SET="$ROOT/Pfhorge Source/Resources/Media.xcassets/PhorgeAppIcon.appiconset"

if [[ ! -f "$MASTER" ]]; then
  echo "error: master icon not found: $MASTER" >&2
  exit 2
fi

mkdir -p "$SET"

make_icon() {
  local pixels="$1"
  local output="$2"
  sips -z "$pixels" "$pixels" "$MASTER" --out "$SET/$output" >/dev/null
}

make_icon 16 icon_16x16.png
make_icon 32 icon_16x16@2x.png
make_icon 32 icon_32x32.png
make_icon 64 icon_32x32@2x.png
make_icon 128 icon_128x128.png
make_icon 256 icon_128x128@2x.png
make_icon 256 icon_256x256.png
make_icon 512 icon_256x256@2x.png
make_icon 512 icon_512x512.png
make_icon 1024 icon_512x512@2x.png

cat > "$SET/Contents.json" <<'JSON'
{
  "images" : [
    {"filename":"icon_16x16.png","idiom":"mac","scale":"1x","size":"16x16"},
    {"filename":"icon_16x16@2x.png","idiom":"mac","scale":"2x","size":"16x16"},
    {"filename":"icon_32x32.png","idiom":"mac","scale":"1x","size":"32x32"},
    {"filename":"icon_32x32@2x.png","idiom":"mac","scale":"2x","size":"32x32"},
    {"filename":"icon_128x128.png","idiom":"mac","scale":"1x","size":"128x128"},
    {"filename":"icon_128x128@2x.png","idiom":"mac","scale":"2x","size":"128x128"},
    {"filename":"icon_256x256.png","idiom":"mac","scale":"1x","size":"256x256"},
    {"filename":"icon_256x256@2x.png","idiom":"mac","scale":"2x","size":"256x256"},
    {"filename":"icon_512x512.png","idiom":"mac","scale":"1x","size":"512x512"},
    {"filename":"icon_512x512@2x.png","idiom":"mac","scale":"2x","size":"512x512"}
  ],
  "info" : {"author":"xcode","version":1}
}
JSON

echo "Generated complete macOS app icon set in:"
echo "  $SET"
