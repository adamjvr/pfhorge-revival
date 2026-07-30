#!/usr/bin/env python3
from pathlib import Path
import sys
root = Path(sys.argv[1] if len(sys.argv) > 1 else '.').resolve()
manager = (root/'Pfhorge Source/Content/PfhorgeContentManager.inc').read_text()
metal = (root/'Pfhorge Source/Preview/Metal/PfhorgeMetalPreviewView.inc').read_text()
checks = {
    'heap SHA buffer': 'NSMutableData *bufferStorage = [NSMutableData dataWithLength:64U * 1024U];' in manager,
    'no 1 MiB stack SHA buffer': 'uint8_t buffer[1024 * 1024];' not in manager,
    'Apply closes': '[self.window orderOut:nil];' in manager,
    'field editor committed': 'makeFirstResponder:nil' in manager,
    'key window observer': 'selector:@selector(windowDidBecomeKey:)' in metal,
    'deferred first responder': 'self.window.acceptsMouseMovedEvents = YES;' in metal,
    'movement key-up consumed': 'NSNumber *releasedKey' in metal,
}
for name, ok in checks.items(): print(('[PASS] ' if ok else '[FAIL] ') + name)
if not all(checks.values()): raise SystemExit(1)
