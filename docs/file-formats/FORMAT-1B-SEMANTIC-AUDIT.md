# FORMAT-1B — Semantic Field Audit and Canonical Foundation

## Purpose

FORMAT-1A defines the physical Pfhorge Native vNext package. FORMAT-1B inventories
what Pfhorge actually knows and separates authoritative semantics from legacy
serialization machinery, source indexes, caches, editor state, and derived data.

The vNext JSON schema must not simply reproduce the accidental shape of the
legacy `NSKeyedArchiver` object graph.

## Field roles

Every discovered field receives one role:

- `authoritative_game`
- `authoritative_editor`
- `source_provenance`
- `derived`
- `cache`
- `runtime_only`
- `opaque_source`
- `deprecated_compatibility`
- `needs_review`

Unknowns remain `needs_review`; the audit never guesses them away.

## Core rules

### Index/pointer mirrors

Legacy pairs such as:

```text
polygon_index
polygon_object
```

must become one canonical UUID relationship. Original indexes are provenance.

### Precalculated data

Adjacency caches, centers, areas, exclusion zones, neighbor lists and similar
fields are recomputed from authoritative topology. Raw source values may be
retained for round-trip evidence when a codec can prove they remain valid.

### Textures

Canonical surfaces store texture semantics, not editor shadow copies. Packed
Marathon descriptors remain source representation/provenance. The old
`side_texture_definition.textureCollection` and `textureNumber` fields are not
independent truth.

### Media

Media type/range/light/current semantics are authoritative. Legacy `height` is
derived current state; its raw source value may be retained for round-trip
fidelity but must not override canonical liquid semantics.

### Layers and names

Pfhorge layers, custom object names, annotations, and other editor metadata are
first-class native information even though Marathon targets cannot represent all
of it. Filtered arrays such as `layerPoints` are derived views.

## Audit command

```bash
python3 scripts/revival/pfhorge_semantic_audit.py \
  --root . \
  --output-dir RevivalArtifacts/FORMAT-1B
```

Outputs JSON, CSV, and Markdown reports.

## Canonical C++ foundation

FORMAT-1B adds:

```text
Pfhorge Source/Format/Core/
    PfhorgeCanonicalFoundation.hpp
    PfhorgeFormatContract.hpp
```

These are Cocoa-independent and define stable 128-bit identities, provenance,
field roles, loss findings, operation kinds, and format capability descriptors.

They do not yet replace `LELevelData`.

## Regression anchor

`Death by accident` remains the first fidelity fixture:

```text
environment       1 / Lava
structure         collection 18 / Lava
media             type 1 / Lava
landscape         collection 27
```

The legacy Pfhor-metadata/Lava-structure result is a negative conversion/merge
fixture, not permission to rewrite descriptors in the renderer.

## Completion criterion

FORMAT-1B is ready for FORMAT-1C when:

1. persistence-bearing classes are inventoried,
2. every currently coded field is classified or explicitly `needs_review`,
3. high-risk metadata/topology/texture/media/platform fields have explicit rules,
4. runtime/default/cache state is excluded,
5. UUID/source-index transformation rules are documented,
6. the canonical C++ foundation compiles independently,
7. the audit tests and repository scan pass.
