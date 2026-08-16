#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations
import argparse, csv, fnmatch, json, re, subprocess
from datetime import datetime, timezone
from pathlib import Path

FORMAT = "org.pfhorge.semantic-field-audit.v1"
INTERFACE_RE = re.compile(
    r"@interface\s+(?P<class>[A-Za-z_]\w*)\s*:\s*(?P<base>[A-Za-z_]\w*)"
    r"(?P<header>[^{]*?)\{(?P<body>.*?)\n\s*\}",
    re.DOTALL,
)
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//.*?$", re.MULTILINE)
ENCODE_PATTERNS = [
    re.compile(r"\[\s*coder\s+encode\w*\s*:\s*([A-Za-z_]\w*)\b"),
    re.compile(r"\bencode(?:Short|Long|NumInt|Obj)\s*\(\s*coder\s*,\s*([A-Za-z_]\w*)\b"),
]
DECODE_ASSIGN_RE = re.compile(r"\b([A-Za-z_]\w*)\s*=\s*\[\s*coder\s+decode\w*")
CODER_KEY_RE = re.compile(r'static\s+NSString\s*\*\s*const\s+([A-Za-z_]\w*)\s*=\s*@"([^"]+)"')

def strip_comments(text):
    return LINE_COMMENT_RE.sub("", BLOCK_COMMENT_RE.sub("", text))

def git_head(root):
    try:
        p = subprocess.run(["git","rev-parse","HEAD"], cwd=root, text=True,
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True)
        return p.stdout.strip() or None
    except Exception:
        return None

def rel(path, root):
    try: return str(path.relative_to(root))
    except ValueError: return str(path)

def iter_headers(root):
    base = root/"Pfhorge Source"/"Data Objects"
    if not base.is_dir():
        raise SystemExit(f"ERROR: missing Data Objects directory: {base}")
    yield from sorted(base.rglob("*.h"))

def parse_ivar_statements(body):
    cleaned = strip_comments(body)
    cleaned = re.sub(r"@\s*(?:private|protected|public|package)", "", cleaned)
    for raw in cleaned.split(";"):
        stmt = " ".join(raw.strip().split())
        if not stmt or stmt.startswith("#") or "(" in stmt or ")" in stmt:
            continue
        matches = list(re.finditer(
            r"(?:\*+\s*)?([A-Za-z_]\w*)\s*(\[[^\]]+\])?\s*(?=,|$)", stmt
        ))
        if not matches: continue
        for m in matches:
            name = m.group(1)
            if name in {"short","int","long","unsigned","signed","BOOL","id","float","double","char"}:
                continue
            yield {"field":name, "declaration":stmt+";"}

def parse_interfaces(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    out=[]
    for m in INTERFACE_RE.finditer(text):
        out.append((m.group("class"), m.group("base"), list(parse_ivar_statements(m.group("body")))))
    return out

def impls_for(header):
    c=[header.with_suffix(".m"), header.with_suffix(".mm")]
    if header.name=="LELevelData.h": c += sorted(header.parent.glob("LELevelData-*.m"))
    if header.name=="LELine.h": c += sorted(header.parent.glob("LELine-*.m"))
    return [p for p in c if p.is_file()]

def coding_usage(paths):
    encoded=set(); decoded=set(); keys={}
    for p in paths:
        t=strip_comments(p.read_text(encoding="utf-8", errors="replace"))
        for pattern in ENCODE_PATTERNS: encoded.update(pattern.findall(t))
        decoded.update(DECODE_ASSIGN_RE.findall(t))
        keys.update(dict(CODER_KEY_RE.findall(t)))
    return encoded,decoded,keys

def load_rules(path):
    obj=json.loads(path.read_text(encoding="utf-8"))
    if obj.get("version")!=1: raise SystemExit("ERROR: unsupported policy version")
    return obj["rules"]

def classify(cls, field, encoded, decoded, rules):
    key=f"{cls}.{field}"
    for r in rules:
        if fnmatch.fnmatchcase(key,r["pattern"]):
            x=dict(r); x["rule"]=r["pattern"]; return x
    if field.startswith("theMap") and field.endswith("ST"):
        return {"role":"runtime_only","storageDomain":"none","canonicalTransform":"none",
                "roundTripPolicy":"discard","rationale":"Borrowed runtime lookup array heuristic.",
                "rule":"heuristic:runtime-ST"}
    if field.endswith("_object") or field.endswith("Object"):
        return {"role":"derived","storageDomain":"none","canonicalTransform":"uuid-reference",
                "roundTripPolicy":"recompute","rationale":"Pointer mirror heuristic.",
                "rule":"heuristic:pointer-mirror"}
    return {"role":"needs_review","storageDomain":"unknown","canonicalTransform":"review",
            "roundTripPolicy":"review","rationale":"No explicit rule; unresolved intentionally.",
            "rule":"unclassified"}

def risk(role, encoded, decoded, field):
    n=(4 if encoded or decoded else 0)+(5 if role=="needs_review" else 0)
    if role in {"derived","source_provenance","deprecated_compatibility"}: n+=2
    if any(x in field.lower() for x in ("texture","media","environment","polygon","line","side","platform","light","index","flag","permutation")): n+=2
    return n

def audit(root, policy):
    rules=load_rules(policy); rows=[]; classes={}
    for header in iter_headers(root):
        for cls,base,fields in parse_interfaces(header):
            if not fields: continue
            impls=impls_for(header); enc,dec,keys=coding_usage(impls)
            classes[cls]={"base":base,"header":rel(header,root),
                          "implementations":[rel(p,root) for p in impls],"coderKeys":keys}
            for item in fields:
                pol=classify(cls,item["field"],item["field"] in enc,item["field"] in dec,rules)
                row={"class":cls,"baseClass":base,"field":item["field"],
                     "declaration":item["declaration"],"sourceFile":rel(header,root),
                     "encoded":item["field"] in enc,"decoded":item["field"] in dec,**pol}
                row["risk"]=risk(row["role"],row["encoded"],row["decoded"],row["field"])
                rows.append(row)
    counts={}
    for r in rows: counts[r["role"]]=counts.get(r["role"],0)+1
    return {"format":FORMAT,"generatedAt":datetime.now(timezone.utc).isoformat(),
            "source":{"root":str(root),"gitCommit":git_head(root),"policy":rel(policy,root)},
            "summary":{"classCount":len(classes),"fieldCount":len(rows),"roleCounts":counts,
                       "encodedNeedsReview":sum(1 for r in rows if (r["encoded"] or r["decoded"]) and r["role"]=="needs_review")},
            "classes":classes,
            "fields":sorted(rows,key=lambda r:(r["class"].lower(),r["field"].lower()))}

def validate_minimums(report):
    s=report["summary"]
    if s["classCount"]<12: raise SystemExit(f"ERROR: audit found only {s['classCount']} classes")
    if s["fieldCount"]<80: raise SystemExit(f"ERROR: audit found only {s['fieldCount']} fields")
    lookup={(r["class"],r["field"]):r for r in report["fields"]}
    required={
        ("LELevelData","environment_code"):"authoritative_game",
        ("LELevelData","points"):"authoritative_game",
        ("LEPolygon","floor_texture"):"authoritative_game",
        ("LEPolygon","adjacent_polygon_indexes"):"derived",
        ("LESide","primary_texture"):"authoritative_game",
        ("PhMedia","type"):"authoritative_game",
        ("PhMedia","height"):"derived",
        ("PhLight","light_states"):"authoritative_game",
        ("PhPlatform","static_flags"):"authoritative_game",
    }
    for k,expected in required.items():
        row=lookup.get(k)
        if not row: raise SystemExit(f"ERROR: required field not discovered: {k[0]}.{k[1]}")
        if row["role"]!=expected: raise SystemExit(f"ERROR: bad role for {k[0]}.{k[1]}: {row['role']}")

def write_csv(report,path):
    cols=["class","baseClass","field","role","storageDomain","canonicalTransform","roundTripPolicy",
          "encoded","decoded","risk","rule","rationale","sourceFile","declaration"]
    with path.open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=cols,extrasaction="ignore"); w.writeheader(); w.writerows(report["fields"])

def write_md(report,path):
    s=report["summary"]; review=sorted([r for r in report["fields"] if r["role"]=="needs_review"],
                                      key=lambda r:(-r["risk"],r["class"],r["field"]))
    high=sorted(report["fields"],key=lambda r:(-r["risk"],r["class"],r["field"]))[:50]
    L=["# Pfhorge FORMAT-1B Semantic Field Audit","",
       f"- Git commit: `{report['source']['gitCommit'] or 'unknown'}`",
       f"- Classes: **{s['classCount']}**",f"- Fields: **{s['fieldCount']}**",
       f"- Encoded/decoded fields needing review: **{s['encodedNeedsReview']}**","",
       "## Role totals","","| Role | Count |","|---|---:|"]
    for k in sorted(s["roleCounts"]): L.append(f"| `{k}` | {s['roleCounts'][k]} |")
    L += ["","## Highest-risk fields","","| Risk | Class | Field | Role | Encoded |",
          "|---:|---|---|---|---|"]
    for r in high: L.append(f"| {r['risk']} | `{r['class']}` | `{r['field']}` | `{r['role']}` | {'yes' if r['encoded'] else 'no'} |")
    L += ["","## Needs review",""]
    if not review: L.append("No fields remain unclassified.")
    else:
        L += ["| Risk | Class | Field | Coded | Declaration |","|---:|---|---|---|---|"]
        for r in review[:200]:
            d=r["declaration"].replace("|","\\|")
            L.append(f"| {r['risk']} | `{r['class']}` | `{r['field']}` | {'yes' if r['encoded'] or r['decoded'] else 'no'} | `{d}` |")
    path.write_text("\n".join(L)+"\n",encoding="utf-8")

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--root",default=".")
    ap.add_argument("--output-dir",default="RevivalArtifacts/FORMAT-1B"); ap.add_argument("--policy")
    ap.add_argument("--no-minimum-checks",action="store_true"); a=ap.parse_args()
    root=Path(a.root).expanduser().resolve()
    policy=Path(a.policy).expanduser().resolve() if a.policy else root/"scripts/revival/format1b_field_policy.json"
    out=Path(a.output_dir); out=out if out.is_absolute() else root/out; out.mkdir(parents=True,exist_ok=True)
    report=audit(root,policy)
    if not a.no_minimum_checks: validate_minimums(report)
    (out/"semantic-field-audit.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    write_csv(report,out/"semantic-field-audit.csv"); write_md(report,out/"semantic-field-audit.md")
    s=report["summary"]
    print(f"FORMAT-1B AUDIT: classes={s['classCount']} fields={s['fieldCount']} encoded-needs-review={s['encodedNeedsReview']}")
    print(f"Reports: {out}")
    return 0
if __name__=="__main__": raise SystemExit(main())
