# TEX-1A.2 — Wall surface completeness

## Why this phase blocks the commit

VM-4A builds and its collision/door work is useful, but first-person Visual
Mode still fails to texture every expected wall. The phase remains uncommitted
until the renderer can distinguish a genuinely empty map reference from a
surface-linkage, Shapes lookup, image conversion, Metal upload, or draw-pass
failure.

## Confirmed root cause

`LEPolygon-sideIndexesAtIndex:` is not a raw Marathon side-index accessor. Its
implementation derives the result from the cached `side_objects` pointer and
returns **zero** when that pointer is nil.

The provisional VM-4A builder called that method after a nil
`sideObjectAtIndex:` and then treated the returned zero as a valid index into
the level side array. As a result, an unresolved polygon edge silently became
`allSides[0]`. That can make floors and ceilings render normally while walls
use the wrong texture definition or fall back.

TEX-1A.2 removes that fallback. It prefers the side relationships owned by the
line record, accepts a compatible direct polygon-side pointer, then uses
back-reference and full-array recovery. A rejected or unresolved edge is
reported instead of being aliased to side zero.

## Reproduction evidence

A raw Marathon-container audit of the supplied **Detention Center** map found
that its main level, `Minimum wage for THIS?!`, contains 357 polygons, 1,068
lines, and 972 side records. Its wall descriptors consistently use the lava
collection, so a broad environment mismatch does not explain the missing walls.
The same level contains 32 non-empty transparent side texture references; the
pre-TEX-1A.2 builder counted those references but generated zero transparent
wall surfaces.

The map also contains full, high, low, and split side types. That makes it a
useful runtime gate for primary/secondary band selection as well as line-owned
clockwise/counterclockwise side recovery. The user-supplied map remains a local
test fixture and is not redistributed in this package.

## Transparent sides

The classic Marathon side record contains independent primary, secondary, and
transparent texture definitions. The previous Metal builder counted
transparent references but did not create a surface for them.

TEX-1A.2 adds a transparent wall quad across the open portal band, retains the
transparent transfer mode, light, offsets, line/side provenance, and sends the
surface through the existing translucent Metal pass. The decoded Shapes image
alpha remains authoritative.

## Audit boundary

Each preview surface now retains:

- polygon ID
- line ID
- side ID
- polygon edge index
- floor / ceiling / primary / secondary / transparent / media texture layer
- texture descriptor
- translucent-pass classification

The scene audit counts the exact side-resolution path. The Metal audit
separately counts invalid descriptors, negative-cache hits, repository
failures, missing images, image-conversion failures, and upload failures.

## Aleph One provenance

This implementation was independently written against the public Marathon
structures documented by the GPL-licensed Aleph One source:

- `Source_Files/GameWorld/map.h`
- `line_data.clockwise_polygon_side_index`
- `line_data.counterclockwise_polygon_side_index`
- `_full_side`, `_high_side`, `_low_side`, `_composite_side`, `_split_side`
- `side_data.primary_texture`
- `side_data.secondary_texture`
- `side_data.transparent_texture`

The source defines transparent textures as a separate side texture that is not
drawn when its descriptor is empty. No Aleph One implementation code was
copied into Pfhorge.

## Deferred

Composite/control-panel inset overlays, animated transfer modes, full light
state evaluation, and the separate companion Visual Mode Texture Palette
window remain in later roadmap phases. The existing 2D Texture Inspector is
unchanged and remains canonical.
