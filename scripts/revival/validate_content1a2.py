#!/usr/bin/env python3
from pathlib import Path
import py_compile, sys

root=Path(sys.argv[1] if len(sys.argv)>1 else '.').resolve()
cm=(root/'Pfhorge Source/Content/PfhorgeContentManager.inc').read_text()
settings=(root/'Pfhorge Source/Content/PfhorgeVisualModeSettings.h').read_text()
metal=(root/'Pfhorge Source/Preview/Metal/PfhorgeMetalPreviewView.inc').read_text()
make=(root/'revival.mk').read_text()
required_cm=[
    'Base game data / Shapes — Required',
    'Enhanced texture appearance — Optional',
    'Install Original + Enhanced',
    'PFHORGE_PROGRESS ',
    'Copy Path',
    'Find Existing…',
    'Remove Enhanced Texture Profile?',
    'NSMenu *contentMenu',
    'Active Appearance',
    'Display Maximum',
]
for marker in required_cm:
    if marker not in cm: raise SystemExit(f'CONTENT-1A.2 validation failed: Content Manager marker missing: {marker}')
if 'Advanced Actions' in cm:
    raise SystemExit('CONTENT-1A.2 validation failed: obsolete Advanced Actions menu remains')
for marker in ['PfhorgeVMFrameRateDisplayMaximum', 'PfhorgeVisualModeMaximumFrameRateForScreen', 'PfhorgeResolvedVisualModeFrameRate']:
    if marker not in settings: raise SystemExit(f'CONTENT-1A.2 validation failed: settings marker missing: {marker}')
for marker in ['CONTENT-1A.2 dynamic display frame rate', 'PfhorgeResolvedVisualModeFrameRate', 'NSWindowDidChangeScreenNotification']:
    if marker not in metal: raise SystemExit(f'CONTENT-1A.2 validation failed: Metal marker missing: {marker}')
if 'content1a2-check:' not in make:
    raise SystemExit('CONTENT-1A.2 validation failed: make target missing')
for game in ['marathon','marathon2','infinity']:
    builder=root/'scripts/revival/content-builders'/game/'build_pack.py'
    py_compile.compile(str(builder),doraise=True)
    text=builder.read_text()
    for marker in ['def emit_progress(', 'PFHORGE_PROGRESS ', '"download"', '"package"', '"complete"']:
        if marker not in text: raise SystemExit(f'CONTENT-1A.2 validation failed: {game} builder marker missing: {marker}')
print('CONTENT-1A.2 / VM-SETTINGS-1B.1 portable validation passed')
