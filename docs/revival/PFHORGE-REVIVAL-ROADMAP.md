# Pfhorge Revival — Integrated Roadmap

## Direction

Pfhorge Revival is turning the surviving native macOS Pfhorge codebase into a
maintained Marathon-family editor without discarding the map semantics, editor
behavior, or historical compatibility that made the original application
useful.

The project now has three concrete layers moving forward:

```text
Pfhorge editor/document model
        |
        +--> map intake, validation, save/export
        |
        +--> content and Shapes management
        |
        `--> renderer-neutral PreviewScene
                    |
                    +--> Metal Visual Mode on macOS
                    `--> Vulkan later
```

The goal is a modern editor that still understands the old data instead of a
new UI wrapped around lossy conversion.

## Architectural principles

1. Map data and visual assets are separate.
2. Historical input is never silently overwritten.
3. File contents are authoritative; extensions and classic Finder metadata are
   hints.
4. Container version, map dialect, and supported semantics are independent
   classifications.
5. Unknown or unsupported data must produce an explicit preservation/loss
   result.
6. Optional content is acquired at runtime after user approval, never during
   compilation.
7. Pfhorge never modifies external Marathon/Aleph One installations in place.
8. Original Shapes content remains the canonical fallback for texture data.
9. The renderer consumes immutable preview state and never becomes the owner of
   the live editor model.
10. Every Visual Mode edit eventually passes back through Pfhorge's undoable
    document operations.
11. Correct Marathon behavior outranks generic 3D-engine convenience.
12. `main` is the accepted development line; `experimental` is reserved for
    intentionally risky work.

# Current state — August 2026

## Baseline restoration — complete

The revival has a reproducible modern macOS/Xcode baseline and launches natively
on Apple Silicon.

Completed work includes:

- imported historical source and provenance
- modern Xcode baseline
- dead build-path cleanup
- revival validation scripts and reports
- GPL-3.0-or-later project policy
- startup/support-directory initialization

## Application modernization — working

The application now has:

- persistent splash/contributor screen
- Start Center with create/open/recent workflows
- unified native preferences
- General editor behavior and grid/snapping controls
- saved Colors & Themes palettes
- special polygon color-coding controls
- object visibility settings
- Visual Mode & GPU settings
- rebindable Visual Mode controls
- display/GPU controls up to high-refresh displays
- Content menu and Content Manager foundation

The target is modernization **without** replacing the core editor model merely
for aesthetic reasons.

## Universal map intake — foundation working

Implemented foundations include:

- raw Marathon map container probing
- AppleSingle
- AppleDouble
- MacBinary
- native resource-fork intake
- bounds-checked container inspection
- map/container/dialect classification
- merged scenario detection
- selectable level import
- new native Pfhorge documents
- source provenance snapshots
- import reports
- opaque-data preservation accounting
- corpus reporting and smoke tests

Further dialect and overlay coverage remains later work.

## Content management — foundation working

The revival now includes a distribution-aware content layer for Marathon-family
assets.

Implemented foundations include:

- Content Manager available without a document
- original Shapes discovery and activation
- use-in-place and managed-content concepts
- official trilogy content builders/recipes
- content manifests and provenance
- texture profile infrastructure
- Visual Mode integration through shared settings
- level-environment texture synchronization

Enhanced/HD replacement-profile parity remains later work.

# Visual Mode — current major engineering track

## Completed foundation

The revived Visual Mode currently includes:

- immutable renderer-neutral `PreviewScene`
- Metal `MTKView` backend
- first-person camera
- saved-player-start initialization
- portal-connected visibility
- projected and vertical portal clipping
- floors, ceilings, and split wall geometry
- classic Shapes texture decoding
- Metal texture caching/upload
- configurable filtering and anisotropy
- render scale, MSAA, VSync, and frame-rate controls
- continuous configurable movement
- collision-aware movement
- Use/Open Door interaction
- temporary platform/door state preview
- live synchronization from unsaved editor state
- whole-map orbit diagnostic mode
- runtime diagnostics overlay
- environment-aware texture menus
- unified Forge-inspired texture workspace

## Portal adjacency repair — runtime accepted

The **Death by accident** regression level exposed a concrete historical-data
compatibility problem.

Its archived polygon adjacency arrays were empty even though line records still
retained the real clockwise/counterclockwise polygon ownership. The legacy
polygon adjacency accessor could therefore turn a missing adjacent object into
numeric polygon index zero.

The preview now resolves transparent portal neighbors from `LELine` ownership
first and uses direct polygon adjacency only as a non-nil fallback.

Runtime result:

```text
before
    valid-looking exits could behave like invisible solid walls

after
    normal movement through those portals works
    invisible collision walls at the tested spawn area are gone
```

This is now part of the accepted `main` baseline.

## Forge-style Visual Mode texture workspace — working UI foundation

The Metal viewport and texture palette now live in one Visual Mode window.

Current controls include:

- texture collection
- real Shapes thumbnails
- selected bitmap
- transfer mode
- light
- Apply Textures
- Apply Lights

The palette publishes selection state but intentionally does not mutate map
surfaces yet.

That boundary stays in place until surface provenance/picking is reliable.

# Immediate roadmap

## 1. Surface completeness and side ownership

This is the highest-priority renderer bug.

Some generated wall surfaces still reach Metal without a valid side/texture
descriptor even though the corresponding area appears to require a visible
texture.

The objective is not to hide those surfaces with a guessed texture. We need to
account for them from map topology to draw submission:

```text
polygon edge
    -> line ownership
    -> side
    -> wall band/layer
    -> texture descriptor
    -> Shapes bitmap
    -> Metal texture
    -> visible draw
```

Required work:

- record stable polygon, edge, line, side, and layer provenance
- classify every unresolved wall
- distinguish genuinely side-less topology from resolver failure
- distinguish geometry omission from texture-descriptor failure
- keep invalid/missing references visible in diagnostics
- build regression cases around known problem surfaces

Exit gate: every expected visible wall is either textured correctly or has an
explicit map-semantic reason why it is not.

## 2. Marathon-correct landscapes / sky / space rendering

Landscape transfer mode is currently not faithful.

A Marathon landscape is not ordinary wallpaper mapped across a nearby wall
segment. It is a view-relative distant environment effect.

Required work:

- dedicated Metal landscape path
- camera-relative horizontal coordinates
- correct vertical landscape scale
- correct collection/bitmap handling
- prevent ordinary wall-distance perspective behavior
- fixed-camera comparisons against Aleph One
- verify Marathon 1 / Marathon 2 / Infinity landscape differences where they
  matter

Exit gate: rotating and moving the camera produces the expected distant
landscape behavior rather than stretching the bitmap across individual walls.

## 3. Transfer modes, lighting, transparency, and media

`transferMode` already travels through preview data but Metal must evaluate the
actual effect.

Priorities:

- Normal
- Landscape
- Pulsate
- Wobble / Fast Wobble
- horizontal and vertical slide
- Wander / Fast Wander
- light intensity/state evaluation
- transparent sides
- media surfaces
- texture animation where required

Exit gate: fixed-camera captures are acceptably equivalent to Aleph One for
representative surfaces.

## 4. Reliable surface picking and inspection

Before texture painting, the editor must be able to identify exactly what the
user clicked.

Required provenance:

```text
surface kind
polygon
edge
line
side
texture layer
collection
bitmap
transfer mode
light
```

Required tools:

- hover/click highlight
- surface inspector
- unresolved-wall overlay
- portal-boundary overlay
- surface IDs
- diagnostic ray/triangle or GPU-ID picking
- 2D/3D selection synchronization

Exit gate: every visible editable surface resolves unambiguously to its exact
Pfhorge map field.

## 5. Forge-compatible 3D editing

Once picking is trustworthy, connect the existing texture workspace to the live
document model.

Required workflows:

- texture paint
- eyedropper
- primary / secondary / transparent layer targeting
- floor / ceiling targeting
- light editing
- transfer-mode editing
- offsets
- alignment
- undo/redo
- immediate 2D/3D synchronization
- save/reopen semantic validation

Exit gate: a texture/light edit performed in Visual Mode round-trips through a
native Pfhorge document without ambiguity.

# Following roadmap

## Visibility and geometry hardening

- overlapping/5D-space regression cases
- moving-platform edge cases
- inherited clipping-window validation
- depth ordering
- malformed geometry overlays
- portal-loop protection

## Map dialect coverage

- Marathon 1 historical variants
- Marathon 2
- Marathon Infinity
- known Aleph One extensions
- accepted noncanonical historical records

## Scenario dependencies and overlays

- parent checksum resolution
- overlay application
- scenario folder dependencies
- scripts
- Shapes
- Sounds
- Physics
- resource forks

## Entity preview

- scenery
- items
- weapons
- monsters
- player starts
- animation sequences
- color tables
- optional model replacements

## Explicit safe Marathon export

Ordinary Save remains Pfhorge-native.

Export work will add explicit targets for:

- Marathon 1
- Marathon 2
- Marathon Infinity
- Aleph One-compatible output

Before writing, Pfhorge should report:

- unsupported fields
- compatibility losses
- invalidated opaque data
- checksum/resource consequences

## Playtesting and diagnostics

- temporary export
- Play From Here
- launch Aleph One at a selected polygon
- fixed-camera screenshot regression suite
- map validation overlays
- tag/switch/light/media visualization

## Cross-platform successor

Only after the renderer-neutral behavior is trustworthy:

- Vulkan renderer
- Linux integration
- Windows integration
- shared PreviewScene / PreviewFrame behavior
- cross-backend image tolerances
- optional MoltenVK evaluation

# Productization

Later release work includes:

- automated regression corpus
- CI expansion
- signed/notarized macOS builds
- content update UX
- document migration
- autosave/recovery
- accessibility
- localization
- release packaging

# Definition of success

Pfhorge Revival succeeds when it can open and understand historical content,
edit it with modern native tools, preview Marathon semantics accurately, and
write changes without making the user guess what was lost.

The renderer is part of the editor, not a detached game engine.

The map remains the source of truth.
