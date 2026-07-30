# MAP-1B — Semantic Import, Provenance, and Real-Map Validation

## Scope

MAP-1B converts MAP-1A's safe structural inspection into a transactional editing
workflow and uses authentic imported maps to harden first-person Visual Mode.
It is developed on the existing revival line; this phase does not create another
milestone branch.

## Import workflow

- single-level maps and multi-level merged scenarios are identified separately
- merged scenarios present a selectable level list with directory ordinals and
  logical indexes
- selected levels are written through a temporary staging directory and moved
  into the project only after all level data has been written successfully
- cancellation does not save or dirty the scenario document
- historical source directories trigger an explicit warning when they are also
  the active Pfhorge project directory
- unsafe, empty, duplicate, and colliding level filenames are normalized while
  original internal names remain in the import report

## Source snapshot and loss ledger

Each successful import creates:

```text
.pfhorge-imports/<timestamp>-<uuid>/
    source-data-fork.bin
    source-resource-fork.bin       when present
    import-report.json
```

The report records:

- source path and source envelope
- map kind and dialect
- container/data versions
- declared, computed, and parent checksums
- SHA-256 of preserved fork bytes
- selected directory ordinals and logical indexes
- original and native level names
- structural findings
- per-tag preservation/edit/export status

The complete data-fork snapshot is the initial opaque-data preservation boundary.
Unknown chunks are not claimed export-safe merely because their bytes survive.

## VM-3C.1 camera and visibility work

`PreviewScene` now carries renderer-neutral player starts copied from
`LEMapObject` records. Visual Mode chooses a valid saved-player start before
falling back to a verified polygon interior point.

Portal traversal accepts a preferred seed polygon. This avoids throwing away a
known object-to-polygon relationship and then guessing among overlapping/5D
polygons from coordinates alone.

Portal projection clips opening edges against the near plane before perspective
division. Traversal diagnostics report seed acceptance, portal rejection causes,
visible polygon/surface counts, and emitted vertex counts.

Controls added in this increment:

- `I`: write the current portal diagnostics to the launching Terminal

## Validation

Run:

```bash
make -f revival.mk map-intake-check
make -f revival.mk preview-core-check
make -f revival.mk baseline
```

Then test the local `Detention Center` merged scenario:

1. Confirm it is identified as a three-level merged Marathon 2 scenario.
2. Import only `Minimum wage for THIS?!` into a scenario outside the historical
   source directory.
3. Verify the source data file and AppleDouble sidecar remain unchanged.
4. Open the imported level and enter Metal Visual Mode.
5. Confirm the initial camera reports a player-start source when one exists.
6. Press `I` and record seed, visible-polygon, portal, surface, and vertex counts.
7. Walk through a portal and confirm the seed polygon changes.
8. Press `P` and confirm whole-map orbit mode still renders the complete scene.
9. Exit Visual Mode and confirm camera movement alone does not dirty the document.

## Deferred

- decoding AppleDouble/MacBinary resource forks into project Images/Sounds
- exact export-safe preservation of every unknown chunk
- full edge ordering derived from Aleph One `RenderSortPoly`
- dynamic platform motion and media rendering
- canonical Shapes and texture profiles
