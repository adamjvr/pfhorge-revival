#!/usr/bin/env python3
import json, sys
from pathlib import Path
p=Path(sys.argv[1] if len(sys.argv)>1 else 'RevivalArtifacts/FORMAT-1C/semantic-field-audit.json')
d=json.loads(p.read_text())
s=d['summary']; needs=s.get('roleCounts',{}).get('needs_review',0); coded=s.get('encodedNeedsReview',0)
print(f"FORMAT-1C AUDIT GATE: classes={s['classCount']} fields={s['fieldCount']} needs-review={needs} encoded-needs-review={coded}")
if coded: raise SystemExit('ERROR: encoded/decoded fields remain needs_review')
if needs: raise SystemExit('ERROR: audited fields remain needs_review; schema freeze candidate requires explicit classification')
