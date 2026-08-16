#!/usr/bin/env python3
import argparse, json, zipfile
from pathlib import Path
MIME=b"application/vnd.pfhorge.package+zip"
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("file"); a=ap.parse_args()
    p=Path(a.file).expanduser()
    with zipfile.ZipFile(p) as z:
        names=z.namelist()
        print("FIRST:", names[0] if names else "<empty>")
        if not names or names[0]!="mimetype" or z.read("mimetype")!=MIME:
            raise SystemExit("ERROR: not a Pfhorge Native package")
        m=json.loads(z.read("manifest.json"))
        d=json.loads(z.read(m["document"]))
        lr=d["levels"][0]; l=json.loads(z.read(lr["path"]))
        e=l["extensions"]["org.pfhorge.format2a.snapshot-authority"]
        print("FORMAT:",m["format"],m["formatVersion"])
        print("LEVEL:",l["name"],l["id"])
        print("CANONICAL AUTHORITY:",e["canonicalAuthority"])
        print("AUTHORITATIVE RESOURCE:",e["authoritativeResource"])
        print("COUNTS:")
        for k,v in sorted(e["objectCounts"].items()): print(f"  {k:<18} {v}")
        print("MEMBERS:")
        for n in names: print(" ",n)
    return 0
if __name__=="__main__": raise SystemExit(main())
