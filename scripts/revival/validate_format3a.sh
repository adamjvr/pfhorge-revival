#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "=== Pfhorge FORMAT-3A ==="

echo "[1/5] FORMAT-1C canonical/package foundation"
make -f revival.mk format1c-check

echo "[2/5] FORMAT-3A Python contract tests"
python3 -m py_compile \
  scripts/revival/audit_pfhlev_tree.py \
  scripts/revival/tests/test_format3a_persistence_audit.py

python3 \
  scripts/revival/tests/test_format3a_persistence_audit.py

echo "[3/5] Unified routing/source audit"
python3 - <<'PY'
from pathlib import Path
import plistlib
import re

root = Path(".")

plist_path = (
    root
    / "Pfhorge Source/Resources/"
      "Info-Pfhorge__Upgraded_.plist"
)
plist = plistlib.loads(plist_path.read_bytes())

marathon = None
for item in plist.get(
    "CFBundleDocumentTypes",
    [],
):
    if item.get("CFBundleTypeName") == "Marathon Map":
        marathon = item
        break

if marathon is None:
    raise SystemExit(
        "ERROR: Marathon Map document type missing"
    )

if (
    marathon.get("NSDocumentClass")
    != "PhPfhorgeSingleLevelDoc"
):
    raise SystemExit(
        "ERROR: Marathon Map must route to "
        "PhPfhorgeSingleLevelDoc"
    )

main = (
    root
    / "Pfhorge Source/Other Sources/main.m"
).read_text()

if "PfhorgeInstallUnifiedDocumentController()" not in main:
    raise SystemExit(
        "ERROR: custom document controller "
        "is not bootstrapped"
    )

controller = (
    root
    / "Pfhorge Source/Format/Cocoa/"
      "PfhorgeUnifiedDocumentController.inc"
).read_text()

for marker in [
    "PFHORGE_FORMAT_3A_UNIFIED_DOCUMENT_CONTROLLER",
    "beginOpenPanel:",
    "typeForContentsOfURL:",
    "allowsOtherFileTypes = YES",
]:
    if marker not in controller:
        raise SystemExit(
            f"ERROR: document controller marker "
            f"missing: {marker}"
        )

single = (
    root
    / "Pfhorge Source/View and Controller/"
      "PhPfhorgeSingleLevelDoc.m"
).read_text()

for marker in [
    "PfhorgeOpenMarathonSourceURL",
    "cameFromMarathonFormatedFile = YES",
    "+ (BOOL)autosavesInPlace",
]:
    if marker not in single:
        raise SystemExit(
            f"ERROR: direct-open marker missing: {marker}"
        )

if not re.search(
    r"\+ \(BOOL\)autosavesInPlace\s*"
    r"\{\s*return NO;\s*\}",
    single,
    re.S,
):
    raise SystemExit(
        "ERROR: PhPfhorgeSingleLevelDoc must "
        "disable autosavesInPlace during source migration"
    )

scenario = (
    root
    / "Pfhorge Source/View and Controller/"
      "PhPfhorgeScenarioLevelDoc.m"
).read_text()

if "convertMarathonDataToLevels" not in scenario:
    raise SystemExit(
        "ERROR: Scenario Marathon import still "
        "uses archived-file conversion"
    )

if (
    "PfhorgeNativePackageFromLegacyPfhlevData"
    not in scenario
):
    raise SystemExit(
        "ERROR: Pathways scenario save is not "
        "migrated to native"
    )

workflow = (
    root
    / "Pfhorge Source/Map Intake/Cocoa/"
      "PfhorgeMarathonMapImportWorkflow.inc"
).read_text()

if "PfhorgeNativePackageForLevel" not in workflow:
    raise SystemExit(
        "ERROR: Scenario staging does not "
        "call the native writer"
    )

if "NSArray<LELevelData *> *levels" not in workflow:
    raise SystemExit(
        "ERROR: Scenario staging still accepts "
        "serialized level blobs"
    )

map_data = (
    root
    / "Pfhorge Source/Data Objects/Map-Level Code/"
      "LEMapData.m"
).read_text()

if "PfhorgeLevelFromAnyPfhlevData" not in map_data:
    raise SystemExit(
        "ERROR: merged scenario export is not "
        "native/legacy aware"
    )

if "convertMarathonDataToLevels" not in map_data:
    raise SystemExit(
        "ERROR: direct Marathon-to-level API missing"
    )

lemap = (
    root
    / "Pfhorge Source/View and Controller/LEMap.m"
).read_text()

method = re.search(
    r"- \(NSData \*\)dataOfType:"
    r"\(NSString \*\)aType.*?"
    r"\n\+ \(BOOL\)autosavesInPlace",
    lemap,
    re.S,
)

if (
    not method
    or "PfhorgeNative2ACreatePackage"
       not in method.group(0)
):
    raise SystemExit(
        "ERROR: base LEMap non-Marathon "
        "persistence is not native"
    )

print("FORMAT-3A routing/source audit: PASS")
PY

echo "[4/5] Legacy-write path audit"
python3 - <<'PY'
from pathlib import Path

scenario = Path(
    "Pfhorge Source/View and Controller/"
    "PhPfhorgeScenarioLevelDoc.m"
).read_text()

workflow = Path(
    "Pfhorge Source/Map Intake/Cocoa/"
    "PfhorgeMarathonMapImportWorkflow.inc"
).read_text()

if (
    "selectedLevelData" in scenario
    or "allArchivedLevels" in scenario
):
    raise SystemExit(
        "ERROR: active Scenario Marathon import "
        "still uses archived Pfhorge blobs"
    )

start = workflow.find(
    "static BOOL PfhorgeStageImportedLevelFiles"
)
end = workflow.find(
    "static BOOL PfhorgeKnownEditableMapTag",
    start,
)
stage = workflow[start:end]

if "PfhorgeNativePackageForLevel" not in stage:
    raise SystemExit(
        "ERROR: imported scenario levels are not "
        "native-packaged"
    )

if "[levels[index] writeToFile:" in stage:
    raise SystemExit(
        "ERROR: level objects are being written "
        "as raw bytes"
    )

print(
    "FORMAT-3A native-only active write paths: PASS"
)
PY

echo "[5/5] Full macOS Xcode baseline build"
make -f revival.mk baseline

find scripts/revival \
  -type d \
  -name '__pycache__' \
  -prune \
  -exec rm -rf {} + \
  2>/dev/null || true

echo
echo "FORMAT-3A checks passed."
echo "Runtime File→Open and Scenario Import tests are still required."
