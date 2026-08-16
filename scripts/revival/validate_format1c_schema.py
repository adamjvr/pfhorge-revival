#!/usr/bin/env python3
from __future__ import annotations
import json, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SD=ROOT/'schemas'/'pfhorge-native'
required=['common.schema.json','geometry.schema.json','surfaces.schema.json','world.schema.json','terminals.schema.json','editor.schema.json','provenance.schema.json','level.schema.json','schema-registry.json']
ids=set()
for name in required:
 p=SD/name
 if not p.is_file(): raise SystemExit(f'ERROR: missing schema {p}')
 d=json.loads(p.read_text())
 if name!='schema-registry.json':
  if d.get('$schema')!='https://json-schema.org/draft/2020-12/schema': raise SystemExit(f'ERROR: {name}: wrong Draft')
  if not d.get('$id') or d['$id'] in ids: raise SystemExit(f'ERROR: {name}: missing/duplicate $id')
  ids.add(d['$id'])
print(f'Schema parse/registry check: {len(ids)} schemas OK')
# jsonschema is optional for users, but use it when available.
try:
 import jsonschema
 from referencing import Registry, Resource
except Exception:
 print('jsonschema package not installed; semantic validator remains authoritative for graph checks')
 raise SystemExit(0)
reg=Registry()
for name in required:
 if name=='schema-registry.json': continue
 p=SD/name; contents=json.loads(p.read_text())
 reg=reg.with_resource(p.resolve().as_uri(),Resource.from_contents(contents))
 # relative refs are resolved from file URI, so also register the $id alias
 reg=reg.with_resource(contents['$id'],Resource.from_contents(contents))
level_schema=json.loads((SD/'level.schema.json').read_text())
sample=json.loads((ROOT/'scripts/revival/fixtures/format1c_canonical_sample.json').read_text())
validator=jsonschema.Draft202012Validator(level_schema,registry=reg)
errs=sorted(validator.iter_errors(sample),key=lambda e:list(e.path))
if errs:
 for e in errs[:20]: print('SCHEMA ERROR:',list(e.path),e.message,file=sys.stderr)
 raise SystemExit(1)
print('Draft 2020-12 sample validation: PASS')
