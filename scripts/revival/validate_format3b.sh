#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "=== Pfhorge FORMAT-3B — Canonical Authority ==="

echo "[1/5] FORMAT-3B Python syntax/tests"
python3 -m py_compile \
  scripts/revival/inspect_format2a_package.py \
  scripts/revival/validate_format3b_package.py \
  scripts/revival/tests/test_format3b_canonical_authority.py
python3 scripts/revival/tests/test_format3b_canonical_authority.py

echo "[2/5] Native writer authority audit"
python3 - <<'PY'
from pathlib import Path
codec=Path('Pfhorge Source/Format/Native/PfhorgeNativeDocumentCodec.inc').read_text()
bridge=Path('Pfhorge Source/Format/Native/PfhorgeCanonicalLevelBridge.inc').read_text()
required_codec=[
    '#include "PfhorgeCanonicalLevelBridge.inc"',
    'PFN3BCanonicalLevelJSON',
    'PFN3BValidateWrittenPackage',
    'PFN3BLevelFromCanonicalJSON',
    '@"formatVersion": PFN2AFormatVersion',
]
for marker in required_codec:
    if marker not in codec:
        raise SystemExit(f'ERROR: FORMAT-3B codec marker missing: {marker}')
if '[NSKeyedArchiver archivedDataWithRootObject:level' in codec:
    raise SystemExit('ERROR: FORMAT-3B active writer still serializes LELevelData with NSKeyedArchiver')
# A historical NSKeyedUnarchiver is intentionally retained for 2A/3A migration reads.
for marker in [
    'urn:pfhorge:schema:level:1',
    'org.pfhorge.format3b.canonical-authority',
    '@"canonicalAuthority": @YES',
    '@"legacyBridgeRequired": @NO',
    'PFN3BPolygonEdgeDirection',
    '[level setUpArrayPointersFor:terminal]',
]:
    if marker not in bridge:
        raise SystemExit(f'ERROR: canonical bridge marker missing: {marker}')
if 'PFN3BRefreshLegacyIndexes' in bridge:
    raise SystemExit('ERROR: canonical loader still calls the abstract legacy index-refresh hook')
print('FORMAT-3B native writer authority audit: PASS')
PY

echo "[3/5] FORMAT-1C schema alignment audit"
python3 - <<'PY'
from pathlib import Path
bridge=Path('Pfhorge Source/Format/Native/PfhorgeCanonicalLevelBridge.inc').read_text()
for forbidden in [
    '@"legacyIndex"',
    '@"canonicalAuthority": @YES,\n        @"canonicalModelRevision"',
    '@"schemaVersion"',
]:
    if forbidden in bridge:
        raise SystemExit(f'ERROR: private root/schema field leaked into core JSON: {forbidden}')
# Authority belongs in the namespaced extension, while the public schema stays FORMAT-1C.
if 'PFN3BCanonicalAuthorityExtensionId: @{' not in bridge:
    raise SystemExit('ERROR: canonical authority is not namespaced in level.extensions')

import json
schema=json.loads(Path('schemas/pfhorge-native/geometry.schema.json').read_text())
line_props=schema.get('$defs',{}).get('line',{}).get('properties',{})
for field in [
    'clockwisePolygon',
    'counterclockwisePolygon',
    'clockwiseSide',
    'counterclockwiseSide',
]:
    if field not in line_props:
        raise SystemExit(
            f'ERROR: FORMAT-3B geometry schema is missing line ownership field: {field}'
        )
print('FORMAT-3B FORMAT-1C schema alignment audit: PASS')
PY

echo "[4/5] Legacy bridge is read-only compatibility"
python3 - <<'PY'
from pathlib import Path
codec=Path('Pfhorge Source/Format/Native/PfhorgeNativeDocumentCodec.inc').read_text()
writer=codec[codec.find('static NSData *PfhorgeNative2ACreatePackage'):codec.find('static LELevelData *PfhorgeNative2ALoadLevel')]
reader=codec[codec.find('static LELevelData *PfhorgeNative2ALoadLevel'):]
if 'PFN2ABridgePath' in writer:
    # Read-back assertion may mention the path. Emission is what is forbidden.
    emission='@{\"name\":PFN2ABridgePath' in writer or '@{ @"name": PFN2ABridgePath' in writer
    if emission:
        raise SystemExit('ERROR: FORMAT-3B writer emits bridge/level.archive')
if 'NSKeyedUnarchiver' not in reader or 'PFN2ABridgePath' not in reader:
    raise SystemExit('ERROR: FORMAT-2A/3A bridge migration reader was removed')
print('FORMAT-3B bridge migration policy audit: PASS')
PY

echo "[5/5] FORMAT-3A/FORMAT-1C foundation + macOS Xcode build"
make -f revival.mk format3a-check

find scripts/revival -type d -name '__pycache__' -prune -exec rm -rf {} + 2>/dev/null || true

echo
echo "FORMAT-3B checks passed."
echo "Runtime canonical-authority import/open/edit/save/reopen tests are still required."
