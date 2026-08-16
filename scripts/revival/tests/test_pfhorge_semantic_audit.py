#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import unittest

SCRIPT=Path(__file__).resolve().parents[1]/"pfhorge_semantic_audit.py"
spec=importlib.util.spec_from_file_location("audit",SCRIPT)
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)

class T(unittest.TestCase):
    def test_parser(self):
        body="""
        short environment_code;
        short unsigned objectCount, lineCount, pointCount;
        NSMutableArray<Thing*> *points;
        short vertexIndexes[8];
        __unsafe_unretained Thing *thing_object;
        """
        found={x["field"] for x in m.parse_ivar_statements(body)}
        self.assertTrue({"environment_code","objectCount","lineCount","pointCount","points","vertexIndexes","thing_object"} <= found)
    def test_rule_order(self):
        rules=[
          {"pattern":"Thing.owner_object","role":"authoritative_game","storageDomain":"geometry","canonicalTransform":"uuid-reference","roundTripPolicy":"semantic","rationale":"x"},
          {"pattern":"*.*_object","role":"derived","storageDomain":"none","canonicalTransform":"uuid-reference","roundTripPolicy":"recompute","rationale":"y"}]
        self.assertEqual(m.classify("Thing","owner_object",True,True,rules)["role"],"authoritative_game")
    def test_unknown_coded_is_high_risk(self):
        r=m.classify("Thing","mystery",True,True,[])
        self.assertEqual(r["role"],"needs_review")
        self.assertGreaterEqual(m.risk(r["role"],True,True,"mystery"),9)
if __name__=="__main__": unittest.main(verbosity=2)
