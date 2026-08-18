# FORMAT-1C — Canonical JSON Schema Freeze Candidate

## Status

FORMAT-1C is the first complete *schema candidate* for Pfhorge Native vNext.
It is still pre-1.0 and deliberately leaves the Cocoa document save path on the
legacy archive until the native codec/migrator work in FORMAT-2A/2B.

FORMAT-1B measured the current legacy persistence surface at 24 classes and 308
ivars, with 63 encoded/decoded fields still unresolved. FORMAT-1C resolves those
coded fields and classifies the remaining audited implementation state so the
schema is derived from semantics rather than from `NSKeyedArchiver` layout.

## Canonical authority rules

### Geometry

The authoritative topology is:

```text
point UUIDs
    ↓
line(startPoint UUID, endPoint UUID, flags)
    ↓
polygon.edges[] = ordered {line UUID, side UUID|null, direction}
```

FORMAT-1C originally treated legacy line owner pointers as derived state.
FORMAT-3B runtime round-trip testing showed that Pfhorge's clockwise/
counterclockwise polygon and side relationships are required to reconstruct
historical portal topology exactly. The base geometry schema therefore permits
`clockwisePolygon`, `counterclockwisePolygon`, `clockwiseSide`, and
`counterclockwiseSide` on line records; FORMAT-3B canonical-authority revision 3
requires and cross-validates them against polygon edge usage.

Legacy side indexes, polygon adjacency arrays, length, angle, azimuth, centers,
areas, exclusion zones, and neighbor lists remain derived or fidelity-only
state rather than independent canonical truth.

This is intentional: vNext must have one topology, not several caches that can
disagree.

### Objects

A saved Marathon map object stores:

- object type,
- type-dependent index/permutation,
- facing,
- X/Y/Z,
- polygon relationship,
- flags.

Pfhorge's `x32`/`y32` display coordinates are derived from X/Y and are not
native persistence fields.

### Surfaces

A side contains semantic primary/secondary/transparent surface layers. Each
layer carries:

- texture reference,
- texture offset,
- transfer mode,
- light UUID.

Packed Marathon shape descriptors remain codec/source representation. The
editor-added `textureCollection`/`textureNumber` shadow members in the old side
struct are not independent canonical truth.

### Media

Media range/current/light/type semantics are authoritative. The current media
plane height is derived from the media range and driving light.

Native appearance explicitly distinguishes:

```text
mode = type-default
```

from:

```text
mode = explicit
```

so a classic Lava medium can preserve its raw imported descriptor in provenance
while the canonical renderer resolves the actual Lava appearance by media type.
Future/native extensions may deliberately choose an explicit appearance.

### Terminals

Terminal content stores semantic text and style runs. Marathon byte offsets,
byte lengths, and line-count fields that are recomputed by the exporter are not
canonical text identity. The terminal-level lines-per-page setting remains a
semantic field.

The historical XOR/disguised text flag is source representation/provenance, not
the meaning of the text.

### Editor metadata

Pfhorge-native layers, note groups, custom object names, line override settings,
current layer, and level-specific editor options are first-class native editor
metadata. They are not discarded merely because a Marathon target cannot encode
them.

`Save As`/`Export` capability analysis is responsible for reporting such losses.

### Codec/runtime state

`LEMapData`, `PathwaysExchange`, name-menu caches, Cocoa pointers, undo managers,
local project-directory paths, temporary save buffers, and parser cursors are
runtime implementation state. They do not belong in the native document.

## Schema set

```text
schemas/pfhorge-native/
    common.schema.json
    geometry.schema.json
    surfaces.schema.json
    world.schema.json
    terminals.schema.json
    editor.schema.json
    provenance.schema.json
    level.schema.json
```

All entity schemas reject unknown core members with `additionalProperties:false`.
Extensibility belongs in the package extension mechanism instead of silently
sprouting undocumented fields in core objects.

## Stable identities

Every canonical entity has a UUID. External-format indexes are serialization
assignments and/or provenance, never permanent identity.

Item-placement arrays are a special historical case where array position is
semantic. vNext therefore makes the slot explicit instead of relying on a
container ordinal.

## Validation

`pfhorge_canonical.py` performs graph-level checks that JSON Schema alone cannot:

- duplicate UUID rejection,
- typed reference checking,
- dangling reference rejection,
- polygon edge/side reference integrity checks; side `line`/`polygon` compatibility hints are typed but non-authoritative,
- line endpoint checks,
- floor <= ceiling validation,
- editor layer/note-group member checking,
- terminal target checking,
- provenance binding checking.

Run:

```bash
make -f revival.mk format1c-check
```

## Death by accident regression contract

The historical fixture remains:

```text
environment = 1 (Lava)
structural collection = 18 (Lava)
media type = 1 (Lava)
landscape collection = 27
```

The known legacy archive with Pfhor environment metadata plus Lava structural
descriptors is a negative migration fixture. The vNext model keeps environment
metadata and surface descriptors independent; setting environment metadata must
not silently mutate canonical surfaces or media.
