# Visual Mode Revival Roadmap

## Mission

Build a fast, accurate, editor-oriented 3D Visual Mode that reproduces the map
semantics users expect from Forge and Aleph One while remaining safely connected
to Pfhorge's undoable document model.

Visual Mode is not an embedded game. It is:

- a map renderer
- a surface selector
- a texture/light editor
- a topology inspector
- a validation surface
- eventually a bridge to temporary playtesting

## Rules that are not negotiable

1. The renderer never owns live Objective-C map objects.
2. Rendering consumes immutable preview state.
3. Marathon visibility and surface semantics are independent of Metal/Vulkan.
4. Backend APIs stay in backend-specific code.
5. Editing mutations return through Pfhorge and `NSUndoManager`.
6. OpenGL is a behavioral reference, not the new foundation.
7. Metal is the production macOS backend.
8. Vulkan waits until renderer-neutral behavior is trustworthy.
9. Stable IDs/provenance are required for editing.
10. Rendering bugs are diagnosed rather than hidden with guessed textures.

## Current architecture

```text
LELevelData / editor objects
          |
          v
PreviewSceneBuilder
          |
          v
Immutable PreviewScene
          |
          +--> topology
          +--> side ownership
          +--> texture descriptors
          +--> platform state
          +--> diagnostics
          |
          v
PreviewVisibility
          |
          v
PreviewFrame
          |
          +--> Metal renderer
          +--> picking/inspection
          `--> Vulkan renderer later
```

## Current working implementation

### Geometry and camera

Working:

- native `MTKView`
- Retina-aware viewport
- first-person camera
- saved player-start initialization
- configurable FOV/movement
- continuous key-state movement
- fly up/down
- collision-aware movement
- Use/Open Door
- camera reset
- whole-map orbit diagnostic

### Visibility

Working:

- camera polygon lookup
- portal graph
- projected portal traversal
- vertical clipping
- portal-filtered visible frames
- upper/lower wall splitting
- portal diagnostics

The August 2026 `Death by accident` regression test found and fixed a major
portal-adjacency bug. Old documents may contain empty polygon-adjacency caches
while the line records retain correct clockwise/counterclockwise polygon
ownership. Visual Mode now reconstructs portal neighbors from line ownership,
eliminating the tested invisible collision walls.

### Textures

Working:

- classic Shapes decoding
- collection/bitmap descriptors
- wall/floor/ceiling texture coordinates
- `MTLTexture` cache/upload
- nearest/linear/trilinear filtering controls
- anisotropic filtering controls
- environment-aware collections
- missing-texture diagnostics
- live texture updates from unsaved editor state

Still incomplete:

- some wall surfaces have unresolved side/texture provenance
- landscapes are currently rendered with ordinary wall-style UV assumptions
- transfer-mode effects are not yet fully evaluated by Metal
- lighting is not yet Marathon-faithful
- transparent/media/composite behavior remains incomplete

### Unified Forge-style workspace

Working:

- Metal viewport and palette in one `NSWindow`
- Collection popup
- real clickable Shapes thumbnails
- selection highlight
- Texture Mode popup
- Light popup
- Apply Textures / Apply Lights state
- persistent frame position/size
- native AppKit controls

Intentional limitation:

**The palette does not paint the map yet.**

That is deliberate. Surface picking and map-field provenance must be correct
before 3D clicks are allowed to mutate `LESide` or `LEPolygon`.

## Current diagnostics

Visual Mode already exposes runtime counters for:

- visible polygons
- visible surfaces
- portals
- textured/fallback surfaces
- side resolution
- invalid texture lookups
- repository/image/conversion/upload failures
- collision state
- platform/door state
- topology reconstruction

These diagnostics remain part of the normal development renderer until the
remaining topology/texture problems are accounted for.

# Immediate work

## VM-FIDELITY — surface completeness

Goal: explain every wall that should be visible.

For every generated wall surface retain:

```text
stable surface ID
polygon ID
edge index
line ID
side ID
wall band/layer
texture descriptor
transfer mode
light index
```

Classify failures independently:

```text
geometry never generated
side genuinely absent
side resolver failed
descriptor empty
descriptor invalid
Shapes image missing
conversion failed
Metal upload failed
surface not in visible frame
surface draw submission missing
```

Do not collapse these into a generic "fallback texture" count.

Acceptance:

- every expected visible wall is accounted for
- no guessed neighboring texture is used as a cosmetic fix
- diagnostics identify the precise source record

## VM-LANDSCAPE — real sky/space rendering

Goal: reproduce Marathon landscape behavior.

Landscape surfaces need a dedicated render path because they are distant,
view-relative environments rather than ordinary wall textures.

Implement:

- dedicated landscape pipeline/state
- camera/yaw-relative horizontal sampling
- correct vertical scaling
- correct wrap behavior
- correct collection/bitmap source
- fixed-camera comparison fixtures
- engine-family differences where observable

Acceptance:

- camera rotation pans through the landscape correctly
- camera translation does not make the landscape behave like a nearby wall
- representative captures match Aleph One closely enough for editing

## VM-TRANSFER — transfer modes and lighting

Build the common shader/uniform infrastructure once, then implement effects
incrementally.

Initial order:

1. Normal
2. Landscape
3. Pulsate
4. Wobble / Fast Wobble
5. Horizontal Slide / Fast Horizontal Slide
6. Vertical Slide / Fast Vertical Slide
7. Wander / Fast Wander
8. lighting/state evaluation
9. transparency/media
10. remaining specialized effects

Acceptance:

- transfer mode is no longer metadata ignored by the fragment shader
- animated effects use deterministic time inputs in regression tests

## VM-PICK — reliable surface selection

Preferred implementation should be chosen from the current mesh architecture,
not from a predetermined graphics trick.

Candidates:

- CPU ray/triangle intersection using retained preview geometry
- GPU integer-ID attachment
- hybrid path if CPU metadata and GPU selection have different strengths

Regardless of technique, a hit must resolve to:

```text
surface
polygon
edge
line
side
layer
texture descriptor
light
transfer mode
```

Add:

- hover highlight
- click highlight
- surface inspector
- unresolved-wall overlay
- portal-boundary overlay
- optional surface IDs

Acceptance:

- the exact editable field under the cursor is deterministic
- selection survives scene rebuilds by stable ID

## VM-EDIT — Forge-compatible map mutation

Only after VM-PICK passes.

Connect the existing palette to:

- texture paint
- eyedropper
- primary texture
- secondary texture
- transparent texture
- floor texture
- ceiling texture
- light
- transfer mode
- offsets/alignment
- undo/redo
- 2D/3D synchronization

Acceptance:

- edits round-trip through save/reopen
- Undo returns the exact prior map state
- Visual Mode never creates a separate shadow texture state

# Later work

## Geometry/visibility hardening

- 5D-space fixtures
- inherited clipping windows
- portal loops
- moving-platform transitions
- polygon/surface depth ordering
- malformed geometry visualization

## Media and animated surfaces

- liquids
- media height
- translucency
- animated textures
- control panels/composite surfaces

## Entities

- scenery
- items
- weapons
- monsters
- players
- animation sequences
- color tables
- optional model replacements

## Editor diagnostics

- topology overlays
- tag/switch/light/media visualization
- validation warnings directly in 3D
- measurement tools
- texture-ID/full-bright/light-only modes

## Playtesting

- temporary export
- Play From Here
- launch Aleph One at selected polygon
- preserve temporary files and diagnostics on failure

## Vulkan

Vulkan begins after the Metal/reference behavior is covered well enough that
"same `PreviewFrame`, different backend" is a meaningful test.

Targets:

- Linux
- Windows
- shared renderer-neutral semantics
- cross-backend image tolerances
- optional MoltenVK investigation

# Performance rules

- correctness first
- no full-scene rebuild for a texture-offset-only edit
- dirty tracking by stable IDs
- cache GPU resources by content identity
- avoid synchronous full-frame readback
- if GPU picking is used, read only the requested pixel
- profile before adding complex batching

# Regression strategy

Each visual fixture should record:

- source/provenance
- expected Marathon engine family
- camera polygon
- position
- yaw/pitch
- visible polygons
- visible surfaces
- selected surface for known screen coordinates
- texture/transfer/light state
- reference image when redistribution permits it

Important categories:

- portal visibility
- clipping
- side ownership
- wall bands
- texture coordinates
- landscapes
- lighting
- transfer modes
- media
- sprites
- picking
- save/reopen semantics

The current **Death by accident** map is the primary runtime regression fixture
for portal movement and the remaining surface/landscape fidelity work.

# Non-goals

- embedding the complete Aleph One game
- monster AI
- weapon/HUD simulation
- replacing Pfhorge's map model during renderer work
- rewriting the application in Swift before behavior is covered
- forcing Vulkan into the first reliable macOS implementation
