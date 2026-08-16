#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
echo "=== Pfhorge FORMAT-2A ==="
echo "[1/4] FORMAT-1C foundation"
make -f revival.mk format1c-check
echo "[2/4] Package/required-extension contract"
python3 scripts/revival/tests/test_format2a_contract.py
echo "[3/4] Source integration markers"
python3 - <<'PY'
from pathlib import Path
doc=Path("Pfhorge Source/View and Controller/PhPfhorgeSingleLevelDoc.m").read_text()
inc=Path("Pfhorge Source/Format/Native/PfhorgeNativeDocumentCodec.inc").read_text()
for x in ["PfhorgeNativeDocumentCodec.inc","PfhorgeNative2ACreatePackage(self, theLevel, outError)",
          "PfhorgeNative2ADataLooksLikePackage(data)","PfhorgeNative2ALoadLevel(data, self, outError)"]:
 if x not in doc: raise SystemExit("ERROR missing document marker: "+x)
for x in ["PFHORGE_FORMAT_2A_NATIVE_DOCUMENT_BRIDGE","org.pfhorge.format2a.snapshot-authority",
          "requiredForRead", "bridge/level.archive","PFN2AWriteZip","PFN2AReadZip"]:
 if x not in inc: raise SystemExit("ERROR missing codec marker: "+x)
print("FORMAT-2A source audit: PASS")
PY
echo "[4/4] macOS Xcode baseline build"
make -f revival.mk baseline
find scripts/revival -type d -name '__pycache__' -prune -exec rm -rf {} + 2>/dev/null || true
echo
echo "FORMAT-2A checks passed."
echo "NOTE: runtime legacy-save-native-close-reopen testing is still required."
