# VM-3B: projected portal clipping

## Status

This increment replaces whole-map rendering in the Metal preview with a
renderer-neutral `PreviewFrame` assembled from directed portal openings.

The implementation now provides:

- transparent-line adjacency copied into the immutable scene
- directed portal geometry for each traversable polygon edge
- lower and upper wall segments around vertical portal openings
- camera-polygon lookup
- first-person portal traversal
- inherited projected clipping regions
- safe polygon revisits through materially different clipping regions
- disconnected-overlap rejection
- whole-scene orbit fallback when the camera is outside map space

## Controls

The Metal preview starts in first-person portal mode.

- mouse drag: look
- `W`, `A`, `S`, `D`: move
- `Q`, `E`: move vertically
- scroll: adjust movement speed
- `R`: reset inside the first valid polygon
- `P`: toggle first-person portal mode and whole-scene orbit diagnostic mode

## Rendering behavior

A transparent shared line generates a directed portal when the two polygons
have a non-empty vertical overlap:

```text
opening bottom = max(source floor, destination floor)
opening top    = min(source ceiling, destination ceiling)
```

The source boundary is emitted as:

```text
optional lower wall
+ portal opening
+ optional upper wall
```

Solid lines, missing adjacent polygons, and empty vertical overlaps remain full
walls.

## Aleph One relationship

The traversal is an independently written floating-point editor adaptation
informed by Aleph One's visibility-tree invariants:

- start from the camera polygon
- cross only transparent polygon transitions
- narrow inherited clipping windows
- allow distinct visits to one polygon through different windows

No Aleph One map globals, automap mutation, fixed-point ray caster, object
placement, or rasterizer implementation is copied into Pfhorge.

Pinned study revision:

```text
4cd8346e1c51dbba48434ccd301d73794f16e086
```

## Intentional limitations

- Portal projection currently derives a conservative rectangular clip from the
  four opening corners.
- Near-plane crossing uses visible corners rather than polygon clipping.
- Portal ordering is breadth-first; final Marathon polygon depth ordering is
  deferred to the RenderSortPoly adaptation.
- Dynamic platforms and variable-elevation lines are represented only by the
  current editor snapshot.
- Side textures are not yet assigned to split wall sections.
- Concave floor and ceiling polygons still use diagnostic triangle fans.

## Validation

Run:

```bash
make -f revival.mk preview-core-check
make -f revival.mk baseline
```

Then open a map in Metal Visual Mode and confirm:

1. the camera starts inside the first valid polygon;
2. a doorway reveals the connected polygon;
3. looking away from the doorway culls the connected polygon;
4. raised floors create lower wall sections;
5. lower ceilings create upper wall sections;
6. `P` restores the whole-map orbit diagnostic;
7. legacy OpenGL Visual Mode remains available when Metal preview is disabled.

## Next milestone

VM-3C will replace conservative portal rectangles with edge-aware clipping,
add diagnostic portal outlines, and adapt Aleph One polygon depth ordering.
