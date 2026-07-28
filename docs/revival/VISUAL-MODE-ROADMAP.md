# Visual Mode revival roadmap

## Mission

Create a fast, accurate, editor-oriented 3D Visual Mode that matches the
observable map behavior of Forge and Aleph One while remaining safely integrated
with Pfhorge's undoable editor model.

The preview is not a complete embedded game. It is a map renderer, selector,
texture editor, geometry inspector, and validation surface.

## Architectural rules

1. The renderer never owns or mutates live Objective-C map objects.
2. Rendering consumes an immutable `PreviewScene` snapshot.
3. Marathon visibility and surface semantics are independent of Metal, Vulkan,
   OpenGL, AppKit, and SDL.
4. Backend APIs appear only in backend-specific directories.
5. All editor mutations pass through Pfhorge commands and undo management.
6. OpenGL is a temporary reference, not the new foundation.
7. Metal is the first production backend.
8. Vulkan begins only after renderer-neutral behavior is validated.
9. Exact Aleph One provenance is recorded before adapting implementation code.
10. Visual correctness is tested with fixtures and fixed camera captures.

## Target architecture

```text
LEMapData and editor objects
          |
          v
PreviewSceneBuilder.mm
          |
          v
Immutable PreviewScene
          |
          +--> MarathonVisibility
          +--> MarathonSurfaceBuilder
          +--> MarathonTextureSemantics
          +--> PreviewValidation
          |
          v
PreviewFrame
          |
          +--> MetalPreviewRenderer
          +--> ReferencePreviewRenderer
          `--> VulkanPreviewRenderer (later)
```

## Core data model

The renderer-neutral layer should represent:

- points and lines
- sides
- polygons and adjacency
- floor and ceiling heights
- texture descriptors and offsets
- transfer modes
- light assignments
- media
- platforms and platform extents
- scenery and map objects
- editor-stable IDs
- diagnostic annotations

A preview surface carries the minimum information needed to draw and select it:

- stable surface ID
- owner polygon/side/object ID
- surface kind
- vertices and indices
- texture descriptor
- transfer mode
- light index
- selection mask
- diagnostic flags

## Visibility

Marathon's portal-connected and overlapping-space behavior must not be replaced
with a naïve whole-level mesh. Visibility begins in the camera polygon and
traverses connected portals using clipping windows.

The initial Aleph One study/adaptation scope includes the concepts represented
by `RenderVisTree`, `RenderSortPoly`, and `RenderRasterize`. The integration
boundary emits renderer-neutral surfaces rather than immediate OpenGL calls.

## Rendering backends

### Metal

The first production backend uses `MTKView` embedded in the existing AppKit
window hierarchy.

Initial passes:

1. opaque floors, ceilings, and walls
2. landscapes and special background surfaces
3. transparent media and transfer modes
4. sprites and scenery
5. editor overlays and diagnostics
6. selection outlines
7. integer-ID picking

Initial pipeline families:

- opaque textured
- landscape
- transparent textured
- additive
- tint/static transfer modes
- editor overlay
- picking

### Vulkan

The Vulkan backend is a later portability stage for Linux and Windows. It must
consume the same `PreviewFrame` representation as Metal.

Do not shape the renderer abstraction like a complete generic graphics API.
Expose only the operations Visual Mode needs.

### Reference renderer

A minimal reference backend may be used for deterministic geometry, visibility,
and picking tests. It need not reproduce every visual effect.

## Interaction model

### Forge parity

- first-person navigation
- select walls, floors, ceilings, and objects
- texture eyedropper
- apply texture
- edit texture offsets
- align adjacent walls
- set transfer mode
- adjust floor and ceiling heights
- edit platform extents
- move and rotate objects
- synchronize selection with 2D mode
- preserve all edits through save/reopen

### Modern improvements

- live surface inspector
- GPU ID picking
- multi-selection
- copy/paste complete surface attributes
- texture flood fill
- alignment chains
- searchable texture browser
- selection outline
- measurement overlay
- portal/clipping visualization
- invalid-reference overlays
- platform motion preview
- media-height preview
- light-only/full-bright/texture-ID render modes
- temporary export and “Play From Here” in Aleph One

## Milestones

### VM-0 — preserve current behavior

- capture current OpenGL Visual Mode behavior
- document controls and known failures
- retain the existing implementation behind a legacy switch
- add representative visual fixtures

Exit gate: existing behavior can be compared against the replacement.

### VM-1 — preview-core foundation

- add renderer-neutral IDs and primitive types
- add immutable `PreviewScene`
- add `PreviewRenderer` interface
- add standalone compile validation
- document thread and ownership rules

Exit gate: the preview core compiles without AppKit, Metal, Vulkan, or OpenGL.

### VM-2 — read-only Metal room

- embed `MTKView`
- camera and projection
- walls, floors, and ceilings
- depth buffer
- placeholder textures
- resize and Retina correctness

Exit gate: a simple room from Pfhorge data renders reliably.

### VM-3 — Marathon visibility

- camera polygon lookup
- portal traversal
- clipping windows
- overlapping-space fixtures
- visible polygon ordering
- void handling

Exit gate: 5D-space fixtures match Aleph One visibility.

### VM-4 — texture and lighting fidelity

- Shapes/texture decoding
- wall/floor/ceiling UV semantics
- light intensity
- landscapes
- media
- animated textures
- transfer modes

Exit gate: fixed-camera captures are acceptably equivalent to Aleph One.

### VM-5 — selection

- integer-ID attachment
- side/floor/ceiling/object IDs
- hover highlight
- click selection
- 2D/3D selection synchronization
- selection persistence during scene rebuilds

Exit gate: every visible editable surface can be selected unambiguously.

### VM-6 — Forge-compatible editing

- texture paint and eyedropper
- offset and alignment editing
- transfer modes
- floor/ceiling height editing
- platform bounds
- object movement and rotation
- undo/redo
- save/reopen validation

Exit gate: core Forge Visual Mode workflows pass semantic round-trip tests.

### VM-7 — diagnostics and playtesting

- diagnostic overlays
- malformed geometry highlighting
- tag/switch/light/media visualization
- temporary export
- launch Aleph One from selected polygon
- screenshot regression suite

Exit gate: Visual Mode is useful for both authoring and map debugging.

### VM-8 — Vulkan portability

- Vulkan backend
- Linux and Windows window integration
- shader-source strategy
- cross-backend golden-image tolerances
- optional MoltenVK evaluation

Exit gate: representative scenes render and select correctly on all targets.

## Performance rules

- do not rebuild the entire scene for a texture-offset change
- track dirty polygons, sides, textures, and objects
- use stable IDs rather than raw object pointers
- keep GPU resources cached by content identity
- avoid synchronous full-frame GPU readback
- read only the clicked picking pixel
- profile before introducing complex batching
- correctness outranks premature rendering optimization

## Testing

Each visual fixture records:

- source/provenance
- expected engine family
- camera polygon
- camera position and yaw/pitch
- visible polygon IDs
- visible surface IDs
- selected surface result for known screen coordinates
- reference image when redistribution allows it

Regression categories:

- visibility
- clipping
- geometry
- texture coordinates
- lighting
- transfer modes
- media
- landscapes
- sprites
- picking
- save/reopen semantics

## Non-goals

- embedding the entire Aleph One game
- simulating monster AI
- implementing weapons or HUD
- replacing the map model during early Visual Mode work
- converting the application to Swift before behavior is covered
- making Vulkan a prerequisite for the first working macOS preview
