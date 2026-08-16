#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations
import argparse, json, re, uuid
from pathlib import Path
from typing import Any

UUID_RE=re.compile(r"^[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")

class CanonicalError(ValueError): pass

def _uuid(v:Any,label:str)->str:
    if not isinstance(v,str) or not UUID_RE.fullmatch(v): raise CanonicalError(f"{label}: canonical UUID required")
    try: p=str(uuid.UUID(v))
    except ValueError as e: raise CanonicalError(f"{label}: invalid UUID") from e
    if p!=v: raise CanonicalError(f"{label}: non-canonical UUID")
    return v

def _list(obj:dict,key:str,label:str):
    v=obj.get(key)
    if not isinstance(v,list): raise CanonicalError(f"{label}.{key}: array required")
    return v

def _dict(obj:dict,key:str,label:str):
    v=obj.get(key)
    if not isinstance(v,dict): raise CanonicalError(f"{label}.{key}: object required")
    return v

def validate_level(level:Any)->dict[str,set[str]]:
    if not isinstance(level,dict): raise CanonicalError("level: object required")
    if level.get("$schema")!="urn:pfhorge:schema:level:1": raise CanonicalError("level.$schema: unsupported")
    _uuid(level.get("id"),"level.id")
    if not isinstance(level.get("name"),str): raise CanonicalError("level.name: string required")
    meta=_dict(level,"metadata","level"); geom=_dict(level,"geometry","level"); surf=_dict(level,"surfaces","level")
    world=_dict(level,"world","level"); terms=_dict(level,"terminals","level"); editor=_dict(level,"editor","level"); prov=_dict(level,"provenance","level")

    categories={
      "point":_list(geom,"points","geometry"), "line":_list(geom,"lines","geometry"), "polygon":_list(geom,"polygons","geometry"),
      "side":_list(surf,"sides","surfaces"), "light":_list(world,"lights","world"), "media":_list(world,"media","world"),
      "platform":_list(world,"platforms","world"), "object":_list(world,"objects","world"), "itemPlacement":_list(world,"itemPlacements","world"),
      "ambientSound":_list(world,"ambientSounds","world"), "randomSound":_list(world,"randomSounds","world"), "tag":_list(world,"tags","world"),
      "annotation":_list(world,"annotations","world"), "terminal":_list(terms,"items","terminals"),
      "layer":_list(editor,"layers","editor"), "noteGroup":_list(editor,"noteGroups","editor")}
    ids={k:set() for k in categories}; all_ids={}
    for kind,arr in categories.items():
        for i,item in enumerate(arr):
            if not isinstance(item,dict): raise CanonicalError(f"{kind}[{i}]: object required")
            ident=_uuid(item.get("id"),f"{kind}[{i}].id")
            if ident in all_ids: raise CanonicalError(f"duplicate UUID {ident}: {all_ids[ident]} and {kind}[{i}]")
            ids[kind].add(ident); all_ids[ident]=f"{kind}[{i}]"
    # Terminal sections are entities too.
    section_ids=set()
    for ti,t in enumerate(categories['terminal']):
        secs=t.get('sections')
        if not isinstance(secs,list): raise CanonicalError(f"terminal[{ti}].sections: array required")
        for si,s in enumerate(secs):
            ident=_uuid(s.get('id') if isinstance(s,dict) else None,f"terminal[{ti}].sections[{si}].id")
            if ident in all_ids: raise CanonicalError(f"duplicate UUID {ident}")
            section_ids.add(ident); all_ids[ident]=f"terminal[{ti}].section[{si}]"
    ids['terminalSection']=section_ids

    def ref(v,kind,label,nullable=False):
        if v is None and nullable: return
        ident=_uuid(v,label)
        if ident not in ids[kind]: raise CanonicalError(f"{label}: dangling/wrong-type {kind} reference {ident}")
    for i,p in enumerate(categories['line']):
        ref(p.get('startPoint'),'point',f"line[{i}].startPoint"); ref(p.get('endPoint'),'point',f"line[{i}].endPoint")
        if p.get('startPoint')==p.get('endPoint'): raise CanonicalError(f"line[{i}]: endpoints must differ")
    side_by_id={s['id']:s for s in categories['side']}
    for i,s in enumerate(categories['side']):
        ref(s.get('line'),'line',f"side[{i}].line"); ref(s.get('polygon'),'polygon',f"side[{i}].polygon")
        for lname in ('primary','secondary','transparent'):
            layer=s.get(lname)
            if layer is None: continue
            if not isinstance(layer,dict): raise CanonicalError(f"side[{i}].{lname}: object/null required")
            ref(layer.get('light'),'light',f"side[{i}].{lname}.light",True)
    for i,p in enumerate(categories['polygon']):
        floor=p.get('floor'); ceil=p.get('ceiling')
        if not isinstance(floor,dict) or not isinstance(ceil,dict): raise CanonicalError(f"polygon[{i}]: floor/ceiling objects required")
        if floor.get('height') is not None and ceil.get('height') is not None and floor['height']>ceil['height']:
            raise CanonicalError(f"polygon[{i}]: floor height exceeds ceiling height")
        ref(floor.get('light'),'light',f"polygon[{i}].floor.light",True); ref(ceil.get('light'),'light',f"polygon[{i}].ceiling.light",True)
        ref(p.get('media'),'media',f"polygon[{i}].media",True); ref(p.get('ambientSound'),'ambientSound',f"polygon[{i}].ambientSound",True); ref(p.get('randomSound'),'randomSound',f"polygon[{i}].randomSound",True)
        edges=p.get('edges')
        if not isinstance(edges,list) or len(edges)<3: raise CanonicalError(f"polygon[{i}].edges: at least 3 required")
        for ei,e in enumerate(edges):
            if not isinstance(e,dict): raise CanonicalError(f"polygon[{i}].edges[{ei}]: object required")
            ref(e.get('line'),'line',f"polygon[{i}].edges[{ei}].line")
            sid=e.get('side')
            if sid is not None:
                ref(sid,'side',f"polygon[{i}].edges[{ei}].side")
                side=side_by_id[sid]
                if side.get('polygon')!=p['id'] or side.get('line')!=e.get('line'):
                    raise CanonicalError(f"polygon[{i}].edges[{ei}]: side ownership does not match polygon/line")
    for i,m in enumerate(categories['media']): ref(m.get('light'),'light',f"media[{i}].light",True)
    for i,p in enumerate(categories['platform']): ref(p.get('polygon'),'polygon',f"platform[{i}].polygon"); ref(p.get('tag'),'tag',f"platform[{i}].tag",True)
    for i,o in enumerate(categories['object']): ref(o.get('polygon'),'polygon',f"object[{i}].polygon",True)
    for i,a in enumerate(categories['annotation']): ref(a.get('polygon'),'polygon',f"annotation[{i}].polygon",True)
    for i,l in enumerate(categories['light']): ref(l.get('tag'),'tag',f"light[{i}].tag",True)

    # Editor members may reference any canonical entity except provenance sources.
    document_ids=set(all_ids)
    names=editor.get('names',{})
    if not isinstance(names,dict): raise CanonicalError('editor.names: object required')
    for k in names: _uuid(k,'editor.names key');
    for k in names:
        if k not in document_ids: raise CanonicalError(f"editor.names: unknown object {k}")
    for i,l in enumerate(categories['layer']):
        for j,m in enumerate(l.get('members',[])):
            _uuid(m,f"layer[{i}].members[{j}]")
            if m not in document_ids: raise CanonicalError(f"layer[{i}].members[{j}]: unknown object {m}")
    for i,g in enumerate(categories['noteGroup']):
        for j,m in enumerate(g.get('members',[])): ref(m,'annotation',f"noteGroup[{i}].members[{j}]")
    cur=editor.get('currentLayer'); ref(cur,'layer','editor.currentLayer',True)
    for i,o in enumerate(editor.get('lineOverrides',[])):
        if not isinstance(o,dict): raise CanonicalError(f"editor.lineOverrides[{i}]: object required")
        ref(o.get('line'),'line',f"editor.lineOverrides[{i}].line")

    # Provenance sources/bindings.
    sources=_list(prov,'sources','provenance'); source_ids=set()
    for i,s in enumerate(sources):
        sid=_uuid(s.get('id') if isinstance(s,dict) else None,f"provenance.sources[{i}].id")
        if sid in source_ids: raise CanonicalError(f"duplicate provenance source UUID {sid}")
        source_ids.add(sid)
    for i,b in enumerate(_list(prov,'bindings','provenance')):
        if not isinstance(b,dict): raise CanonicalError(f"provenance.bindings[{i}]: object required")
        oid=_uuid(b.get('object'),f"provenance.bindings[{i}].object")
        sid=_uuid(b.get('source'),f"provenance.bindings[{i}].source")
        if oid not in document_ids: raise CanonicalError(f"provenance.bindings[{i}]: unknown object {oid}")
        if sid not in source_ids: raise CanonicalError(f"provenance.bindings[{i}]: unknown source {sid}")
    for i,f in enumerate(_list(prov,'opaqueFragments','provenance')):
        if not isinstance(f,dict): raise CanonicalError(f"provenance.opaqueFragments[{i}]: object required")
        sid=_uuid(f.get('source'),f"provenance.opaqueFragments[{i}].source")
        if sid not in source_ids: raise CanonicalError(f"provenance.opaqueFragments[{i}]: unknown source {sid}")

    # Classic environment is metadata only; validator intentionally does NOT compare/remap texture collections.
    env=meta.get('environment')
    if env is not None and not isinstance(env,dict): raise CanonicalError('metadata.environment: object/null required')
    return ids

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('path'); a=ap.parse_args()
    data=json.loads(Path(a.path).read_text(encoding='utf-8'))
    ids=validate_level(data); print(f"VALID canonical level: entities={sum(len(v) for v in ids.values())}")
if __name__=='__main__': raise SystemExit(main())
