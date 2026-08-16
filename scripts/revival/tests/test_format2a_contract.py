#!/usr/bin/env python3
import hashlib,json,tempfile,unittest,zipfile
from pathlib import Path
MIME=b"application/vnd.pfhorge.package+zip"
class T(unittest.TestCase):
 def test_required_bridge_contract(self):
  lid="11111111-1111-4111-8111-111111111111"; did="22222222-2222-4222-8222-222222222222"
  lp=f"levels/{lid}.json"; bridge=b"secure-bridge-placeholder"
  level=(json.dumps({"$schema":"urn:pfhorge:schema:level:1","id":lid,"name":"x",
   "extensions":{"org.pfhorge.format2a.snapshot-authority":{"canonicalAuthority":False,
   "authoritativeResource":"bridge/level.archive","objectCounts":{}}}},sort_keys=True)+"\n").encode()
  doc=(json.dumps({"$schema":"urn:pfhorge:schema:document:1","id":did,"kind":"level",
   "levels":[{"id":lid,"path":lp}],"extensions":["org.pfhorge.format2a.snapshot-authority"]},sort_keys=True)+"\n").encode()
  resources=[]
  for path,data in [("document.json",doc),(lp,level),("bridge/level.archive",bridge)]:
   resources.append({"path":path,"mediaType":"application/octet-stream","sha256":hashlib.sha256(data).hexdigest()})
  manifest=(json.dumps({"$schema":"urn:pfhorge:schema:manifest:1","format":"org.pfhorge.native",
   "formatVersion":"1.0.0-draft.1","kind":"level","document":"document.json",
   "extensions":[{"id":"org.pfhorge.format2a.snapshot-authority","version":"1.0",
   "requiredForRead":True,"requiredForWrite":True,"resources":["bridge/level.archive"]}],
   "resources":resources},sort_keys=True)+"\n").encode()
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/"x.pfhlev"
   with zipfile.ZipFile(p,"w",zipfile.ZIP_STORED) as z:
    for name,data in [("mimetype",MIME),("manifest.json",manifest),("document.json",doc),(lp,level),("bridge/level.archive",bridge)]:
     z.writestr(name,data)
   with zipfile.ZipFile(p) as z:
    self.assertEqual(z.namelist()[0],"mimetype")
    self.assertEqual(z.getinfo("mimetype").compress_type,zipfile.ZIP_STORED)
    m=json.loads(z.read("manifest.json"))
    ext=m["extensions"][0]
    self.assertTrue(ext["requiredForRead"])
    self.assertTrue(ext["requiredForWrite"])
    self.assertEqual(ext["resources"],["bridge/level.archive"])
if __name__=="__main__": unittest.main(verbosity=2)
