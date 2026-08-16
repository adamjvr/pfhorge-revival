# Pfhorge File Format & Interchange Overhaul

This roadmap turns file-format fidelity into a first-class Pfhorge subsystem.

## Architectural invariant

All supported level formats are real codecs around a canonical semantic model.
"Import" is never an alias for "merge some arrays into the current level."

```text
                   Pfhorge document/editor
                            |
                   Canonical semantic model
                            |
        +-------------------+-------------------+
        |                   |                   |
  Pfhorge Native       Marathon/Aleph       Third-party
      codec                codecs               codecs
```

The native Pfhorge format is allowed to grow whenever the canonical model grows.

## FORMAT-1A — Native package foundation

- ZIP + JSON package profile
- `.pfhlev` identity retained
- public schemas
- reference pack/unpack/validate tooling
- security limits
- deterministic output
- extension/provenance/opaque-data rules
- no application save-path switch yet

## FORMAT-1B — Complete canonical semantic audit

Inventory and classify every persisted/convertible value from:

- `LELevelData`
- points
- lines
- polygons
- sides and all texture layers
- lights
- media
- platforms
- objects
- item placement
- ambient/random sounds
- terminals/sections
- tags
- notes
- editor layers/groups/names/settings
- Marathon/Aleph One chunk fields and dialect differences
- Pathways conversion
- Map Intake source snapshots/loss ledger
- external/resource-fork data
- renderer/preview fields

Every value receives one classification:

```text
authoritative canonical
source-format provenance
editor-only canonical
derived/cache (not authoritative)
opaque source
deprecated/legacy compatibility
```

This phase freezes the semantic JSON schema and canonical in-memory contracts.

## FORMAT-1C — Native vNext application codec

- implement `PfhorgeNativeCodec`
- read/write packed package
- legacy `.pfhlev` remains `LegacyPfhorgeLevelCodec`
- explicit legacy -> vNext migration
- transactional staged save
- hash/schema/semantic validation
- package read-back after write
- scenario and single-level documents share one package foundation

## FORMAT-2 — Format registry and document operations

Introduce:

```text
FormatRegistry
FormatCodec
FormatCapabilities
ConversionPlan
LossLedger
ValidationReport
```

Wire:

- Open
- Save
- Save As
- Export Copy
- Validate Document

Normal Save always uses the active codec.

## FORMAT-3 — Import and Merge split

### Import

A complete source level becomes a complete canonical level, including level-wide
metadata.

### Merge

A graph operation with:

- selected root objects
- dependency closure
- UUID remap
- reference rewrite
- collision policy
- environment/texture policy
- light/media/tag/terminal policy
- explicit report

Retire `unionLevel:` as an import mechanism. Keep a compatibility wrapper only
until all callers are migrated.

## FORMAT-4 — External codec hardening

For each supported codec:

- structural probe
- semantic decode
- capability profile
- encode
- validation
- read-back
- semantic diff
- opaque-data preservation policy

Initial priority:

1. Marathon 2 / Infinity WAD family
2. Aleph One extensions
3. Marathon 1 where semantics differ
4. Pathways formats
5. current/legacy Pfhorge
6. third-party formats already advertised by the app

## FORMAT-5 — Corpus and CI

Every writable format receives real fixtures and round-trip assertions.

`Death by accident` is a canonical regression case:

```text
original Marathon 2:
    environment code = 1 (Lava)
    structural collection = 18 (Lava)
    media type = 1 (Lava)
    landscapes = collection 27

required after faithful decode/encode:
    same semantic facts
```

The historical Pfhorge-derived `.pfhlev` with environment 4 plus Lava structural
descriptors remains a negative fixture demonstrating the old merge/import failure.

## UI end-state

```text
File
  Open…
  Save
  Save As…
  Export Copy…
  Validate Document…

Level
  Import Level(s)…
  Merge Content From Level…
```

Before lossy Save As / Export, show a feature-level capability report. Never
silently discard unsupported data.

## Definition of "supported format"

A format is not called writable until the matrix is explicit:

| Capability | Requirement |
|---|---|
| Probe/identify | required |
| Open/decode | required |
| Save | required if advertised editable |
| Save As target | explicit |
| Export target | explicit |
| Validate | required |
| Round-trip class | documented |
| Unknown-data policy | documented |
| Capability/loss report | required for conversion |
| Merge source | canonical decode required |
