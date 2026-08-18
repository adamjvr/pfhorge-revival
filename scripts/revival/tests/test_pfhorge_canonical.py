#!/usr/bin/env python3
from __future__ import annotations
import copy, importlib.util, json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/'revival'/'pfhorge_canonical.py'
FIX=ROOT/'revival'/'fixtures'/'format1c_canonical_sample.json'
spec=importlib.util.spec_from_file_location('pc',SCRIPT); pc=importlib.util.module_from_spec(spec); assert spec.loader; spec.loader.exec_module(pc)
class T(unittest.TestCase):
 def load(self): return json.loads(FIX.read_text())
 def test_sample(self): self.assertIn('polygon',pc.validate_level(self.load()))
 def test_duplicate_uuid_rejected(self):
  d=self.load(); d['geometry']['points'][1]['id']=d['geometry']['points'][0]['id']
  with self.assertRaises(pc.CanonicalError): pc.validate_level(d)
 def test_dangling_endpoint_rejected(self):
  d=self.load(); d['geometry']['lines'][0]['startPoint']='11111111-1111-4111-8111-111111111111'
  with self.assertRaises(pc.CanonicalError): pc.validate_level(d)
 def test_side_hint_mismatch_is_allowed(self):
  # FORMAT-3B authority lives on polygon.edges[*].side.  A side object's
  # line/polygon fields are legacy compatibility hints and may be stale while
  # still referring to valid canonical entities.
  d=self.load(); d['surfaces']['sides'][0]['line']=d['geometry']['lines'][1]['id']
  self.assertIn('polygon',pc.validate_level(d))
 def test_dangling_side_line_hint_rejected(self):
  d=self.load(); d['surfaces']['sides'][0]['line']='11111111-1111-4111-8111-111111111111'
  with self.assertRaises(pc.CanonicalError): pc.validate_level(d)
 def test_dangling_edge_side_rejected(self):
  d=self.load(); d['geometry']['polygons'][0]['edges'][0]['side']='22222222-2222-4222-8222-222222222222'
  with self.assertRaises(pc.CanonicalError): pc.validate_level(d)
 def test_floor_above_ceiling_rejected(self):
  d=self.load(); d['geometry']['polygons'][0]['floor']['height']=2000
  with self.assertRaises(pc.CanonicalError): pc.validate_level(d)
if __name__=='__main__': unittest.main(verbosity=2)
