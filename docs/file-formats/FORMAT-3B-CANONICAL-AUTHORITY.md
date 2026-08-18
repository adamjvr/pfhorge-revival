# FORMAT-3B — Canonical Authority

## Status

FORMAT-3B makes the existing FORMAT-1C canonical JSON graph the authoritative
persistence representation for newly written Pfhorge Native `.pfhlev` files.

This is the phase that removes the FORMAT-2A/3A bootstrap dependency on
`bridge/level.archive` from **new writes**. The historical bridge reader remains
for migration: an older Native package can still be opened, but its next normal
save is emitted as canonical authority.

The public schema is **not replaced** by a new private Cocoa schema. The level
resource continues to declare:

```text
$schema = urn:pfhorge:schema:level:1
```

and follows `schemas/pfhorge-native/*` from FORMAT-1C.

## New authoritative path

```text
LELevelData (live editor graph)
        ↓
FORMAT-1C canonical JSON
        ↓
Pfhorge Native ZIP
        ↓
read-back + graph validation
```

Reading reverses the path:

```text
Pfhorge Native ZIP
        ↓
FORMAT-1C canonical JSON
        ↓
validate stable references
        ↓
materialize LELevelData
        ↓
rebuild derived Cocoa caches
```

The JSON is the authority. Cocoa pointer caches, array-derived indexes, polygon
adjacency caches, point reverse incidence sets, line lengths/angles and other
reconstructible state are not serialized as competing truth. A few historical
Cocoa/Marathon cache values that the current editor/exporter still consumes
(line adjacent-height caches, side exclusion zones, and polygon area/center/
concavity hints) are carried only inside the namespaced `cocoaFidelity`
extension. They are explicitly non-authoritative compatibility hints and may
be regenerated or discarded by future implementations.

## Package shape

A newly written single-level file is expected to contain:

```text
mimetype
manifest.json
document.json
levels/<level-uuid>.json
```

It must **not** contain:

```text
bridge/level.archive
```

The level's `extensions` object includes
`org.pfhorge.format3b.canonical-authority`. That declaration records the model
revision, a diagnostic object-count summary, and a small `cocoaFidelity` bag.
The extension does not supersede the core schema.

## Stable identity

Every persisted entity receives a canonical UUID. References use those UUIDs;
legacy array positions are not persistent identity. Once a Native document is
opened, the UUIDs are attached to the live Cocoa objects so an edit/save cycle
retains identity.

FORMAT-3B assigns identities to:

- points, lines, polygons and sides;
- lights, media and platforms;
- map objects and item-placement slots;
- ambient/random sounds and tags;
- annotations;
- editor layers and note groups;
- terminals and terminal sections.

## Topology

FORMAT-3B keeps polygon edges as the authoritative ordered polygon
boundary and promotes the line ownership relationships required for exact
historical portal reconstruction:

```text
Point UUIDs
   ↓
Line {
  startPoint UUID,
  endPoint UUID,
  flags,
  clockwisePolygon UUID|null,
  counterclockwisePolygon UUID|null,
  clockwiseSide UUID|null,
  counterclockwiseSide UUID|null
}
   ↓
Polygon.edges[] { line UUID, side UUID|null, direction }
```

For canonical-authority revision 3 the four line ownership members are required.
Each polygon edge must name a line for which that polygon is exactly one of the
persisted clockwise/counterclockwise owners. The line-side references are typed
and preserved because Visual Mode and classic-map compatibility depend on them.

`Polygon.edges[].side` remains the authoritative wall-surface binding. A side
object's own `line`/`polygon` members remain compatibility hints and may be stale
without invalidating the canonical edge graph.

Point reverse-line incidence and polygon adjacency arrays are still rebuilt
through existing editor code rather than persisted as additional authority.

## Surfaces and media

Core side surfaces preserve texture reference, offset, transfer mode and light
identity. The old packed Marathon shape descriptor is a codec/Cocoa
representation, not separate semantic truth.

Classic media keeps the FORMAT-1C rule:

```text
appearance.mode = type-default
```

The semantic medium type, range/current/light fields and transfer mode live in
core JSON. Current Cocoa fields whose values are useful for exact legacy export
but are not canonical meaning (for example the packed raw media descriptor and
the cached media height) are retained in `cocoaFidelity`.

Environment metadata remains independent of textures. Loading canonical JSON
sets the stored classic environment code without invoking the legacy editor
setter that can remap structural textures.

## Editor metadata

The canonical editor section persists the Pfhorge-only state that external
Marathon formats cannot necessarily represent:

- object names by UUID;
- layers and layer membership;
- note groups and membership;
- permanent line overrides;
- current layer;
- level-specific primitive options.

These are first-class Native semantics. Export capability analysis—not Native
persistence—decides what a target format can retain.

## Terminals

Terminal and section text is persisted as semantic Unicode text plus style runs.
Legacy byte offsets and cached line-count fields are not canonical text
identity. FORMAT-3B reconstructs an attributed Cocoa string when loading.

The small fidelity extension retains historical terminal flags and implementation
fields needed by the current exporter while terminal text/styles remain the
core authority.

## Compatibility

FORMAT-3B keeps the same public `PfhorgeNative2A*` function names at current
Cocoa call sites to avoid unrelated document-controller churn. Their behavior is
upgraded internally.

Read behavior is:

```text
FORMAT-2A/3A package with bridge/level.archive
    → secure keyed-unarchive migration reader
    → live LELevelData
    → next Save writes FORMAT-3B canonical authority

FORMAT-3B package
    → canonical JSON reader
    → live LELevelData
```

The historical bridge is therefore an **input compatibility path**, not an
output format.

## Save validation

Before the writer returns bytes it validates the canonical graph. It then writes
the complete ZIP in memory and immediately reads the package back, checking:

- Pfhorge Native mimetype;
- manifest/resource hashes;
- document/level resource routing;
- canonical authority declaration;
- UUID uniqueness and typed references;
- topology/side ownership;
- floor/ceiling ordering;
- editor reference integrity;
- absence of `bridge/level.archive`.

The repository-side `validate_format3b_package.py` additionally reuses the
existing FORMAT-1C `pfhorge_canonical.py` validator against files produced by the
runtime.

## Regression target

For the canonical `Death by accident` Marathon 2 fixture, a fresh scenario
import should report:

```text
points          375
lines           570
polygons        186
sides           530
lights          7
media           3
platforms       3
objects         115
itemPlacements  128
ambientSounds   4
randomSounds    0
tags            1
annotations     0
```

and:

```text
CANONICAL AUTHORITY: True
CANONICAL MODEL REVISION: 3
LEGACY BRIDGE MEMBER: False
```

The known semantic contract remains Lava environment `1`, Lava structural
collection `18`, Lava media type `1`, and landscape collection `27`.

## Validation

Run:

```bash
make -f revival.mk format3b-check
```

The macOS Xcode build is still the authoritative compile check for the Cocoa
bridge. Runtime import/open/edit/save/reopen tests remain required before this
phase should be committed.
