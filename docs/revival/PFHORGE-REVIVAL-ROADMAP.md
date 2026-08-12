# Pfhorge Revival — Integrated Roadmap

## Direction

Pfhorge Revival is a GPL-3.0-or-later Marathon map editor with a
renderer-neutral preview core, native Metal on macOS, a future Vulkan backend,
universal Marathon-family map intake, classic Mac container recovery,
distribution-aware content management, original and HD texture profiles,
Visual Mode editing, and explicit safe export.

Doom WAD support is not in scope. Public terminology uses **Marathon map
container**; historical Aleph One `wad_*` names remain only where source
compatibility requires them.

## Architectural principles

1. Map data and visual assets are separate.
2. File contents are authoritative; extensions and Finder types are hints.
3. Container version, map data dialect, and semantic feature support are
   independent classifications.
4. Import creates new Pfhorge documents and never overwrites historical input.
5. Unknown data is accompanied by a preservation/export loss ledger.
6. Optional content downloads happen at runtime, never during compilation.
7. Large third-party assets remain outside `Pfhorge.app` and retain upstream
   licensing, attribution, version, and provenance.
8. Original Shapes content is the canonical fallback below optional external
   replacement textures.
9. Existing Marathon installations may be used in place or copied into
   Pfhorge-managed storage; Pfhorge never modifies external installations.
10. Renderer settings and key bindings are persistent, inspectable, and shared
    by the Metal Visual Mode implementation.

## Completed and locally validated foundation

### Application modernization

- modern splash and startup behavior
- support-directory initialization
- current Xcode baseline
- GPL-3.0-or-later revival source conventions
- preliminary branding and icon foundation

### VM-2

- immutable `PreviewScene`
- live editor-to-preview conversion
- native Metal depth rendering
- interactive camera
- renderer-neutral smoke tests
- legacy OpenGL fallback

### VM-3A / VM-3B

- polygon containment and adjacency
- projected portal traversal
- vertical portal clipping
- upper/lower wall splitting
- portal-aware `PreviewFrame`
- first-person camera and whole-map diagnostic mode
- successful live rendering of newly authored geometry

### MAP-1A

- raw, AppleSingle, AppleDouble, MacBinary, and native-resource-fork intake
- bounds-checked Marathon container probe
- content and dialect classification
- Map Identification UI
- JSON, CSV, and Markdown corpus reporting
- extensionless and classic-Mac map selection

### MAP-1B / VM-3C.1

Locally built and runtime-tested on imported historical content:

- merged Marathon scenario detection
- selectable level import
- new native Pfhorge level documents
- source provenance snapshot and import report
- opaque-data preservation ledger
- saved-player-start camera initialization
- verified fallback seed polygon
- near-plane portal clipping
- portal diagnostics through the `I` key
- successful import of the three-level `Detention Center` scenario

Before declaring this milestone repository-complete, commit and push the tested
local tree.

Regression requirement: entering Visual Mode and moving the camera must not
mark an otherwise clean document as edited.

## Active next phase

# CONTENT-1A / VM-SETTINGS-1A — Distribution-Aware Content Manager

This is the immediate next phase.

### Unified Content Manager

Add a first-class dialog available from:

```text
Pfhorge > Content Manager…
```

and linked from:

```text
View > Visual Mode & GPU Settings… > Textures & Content
```

The same dialog manages:

- official Classic Marathon, Marathon 2, and Marathon Infinity distributions
- original Shapes content and embedded original textures
- external texture files bundled with those distributions
- Aleph One plugin folders and archives
- MML texture mappings
- optional M1, M2, and Infinity HD profiles
- custom scenario-specific Shapes and replacement packs

### Distribution discovery and installation

For each Marathon family game, support:

- scan this Mac for existing installations
- choose an existing distribution or scenario directory
- use existing content in place
- copy selected content into Pfhorge-managed storage
- download an approved official distribution after explicit confirmation
- show download and extraction progress
- verify pinned archive hashes
- safely extract with traversal, symlink, case-collision, file-count, and size
  limits
- record source URL, package version, hashes, installed files, license, credits,
  and validation results
- verify, repair, rescan, reveal, and remove managed copies

Managed storage:

```text
~/Library/Application Support/Pfhorge/Content/
├── Downloads/
├── Distributions/
├── Shapes/
├── Texture Profiles/
└── Manifests/
```

Removing a managed copy must never delete an external installation.

### Content profiles

Each distribution scan produces one or more selectable profiles:

```text
Original
Distribution Default
Enhanced
Custom
Untextured Diagnostic
```

Resolver priority:

```text
project override
→ user-selected HD profile
→ enabled plugin bundled with the selected distribution
→ original texture embedded in Shapes
→ missing-texture diagnostic checkerboard
```

Multiple Shapes sources and profiles per game are required for custom
scenarios.

### Visual Mode and GPU settings

Add:

```text
View
├── Visual Mode
└── Visual Mode & GPU Settings…
```

Tabs:

- Controls
- Display & GPU
- Textures & Content
- Diagnostics

Metal Visual Mode must consume the stored settings rather than hardcoded keys.

Default controls:

```text
Forward             W
Backward            S
Strafe left         A
Strafe right        D
Fly down            Q
Fly up              E
Reset camera        R
Orbit diagnostic    P
Diagnostics         I
Mouse drag          Look/orbit
Scroll              Movement speed/orbit distance
```

Required settings include:

- rebindable movement and diagnostic keys
- mouse sensitivity and invert Y
- base movement speed
- continuous key-state movement independent of macOS key repeat
- field of view
- frame-rate limit
- VSync
- render scale
- MSAA
- texture filtering and anisotropy
- Metal device selection where multiple devices exist
- diagnostics overlay options

Existing legacy `NSUserDefaults` values should be migrated where their meanings
match.

### CONTENT-1A acceptance gate

- Content Manager opens without a document.
- Official trilogy cards report Installed, Missing, Invalid, or External.
- Existing installations can be scanned and previewed before selection.
- Use-in-place and managed-copy modes both work.
- No network access occurs during build.
- Downloads happen only after explicit approval.
- Hash verification and safe extraction are enforced.
- Shapes and external distribution textures are cataloged separately.
- Original and replacement profiles can be enabled or disabled.
- Manifests preserve provenance, rights notices, credits, and validation.
- External installations are never modified or deleted.
- Visual Mode settings open from the View menu.
- Metal key bindings, sensitivity, speed, and FOV persist and take effect.
- `P`, `R`, and `I` remain functional and become rebindable.
- Current untextured rendering remains available.

## Following phase

# TEX-1A — Classic Shapes Textures in Metal

After CONTENT-1A can supply validated content:

1. Build the canonical collection/bitmap catalog from Shapes.
2. Convert decoded original images into cached `MTLTexture` objects.
3. Render original floor and ceiling textures.
4. Render full, high, low, split, and composite wall sections.
5. Respect side texture offsets, wrapping, transfer modes, and light indexes.
6. Add landscapes, transparent sides, liquids, and animations.
7. Fall back to original Shapes when a replacement is absent.
8. Show a diagnostic checkerboard for invalid descriptors.

## Later phases

### TEX-1B — External and HD Texture Profiles

- Aleph One MML replacement inventory and parsing
- deterministic profile layering and conflict reports
- M1 Best Available
- M2 CFP
- Infinity CFP
- glow maps and normal maps where supported
- project-local overrides

### TEX-2 — Visual Mode Texture Editing

- surface picking
- wall, floor, and ceiling painting
- eyedropper and palettes
- alignment and offset tools
- primary, secondary, and transparent layers
- lighting and transfer-mode editing
- undo and redo through live document data

### VM-3C — Remaining visibility hardening

- inherited visibility windows
- portal loop protection
- polygon and surface depth ordering
- 5D-space validation
- moving-platform validation

### MAP-2 — Full historical dialect coverage

- Marathon 1 and early container version 1
- Marathon 2 and Infinity
- known Aleph One extensions
- historically accepted noncanonical records

### MAP-3 — Overlays and scenario dependencies

- parent checksum resolution
- overlay application
- resource forks and external content
- scenario folders, scripts, Shapes, Sounds, and Physics dependencies

### ENTITY-1 — Object and sprite preview

- scenery, items, weapons, monsters, players
- animation sequences and color tables
- optional 3D model replacements

### EXPORT-1 — Explicit safe Marathon export

- target-specific M1, M2, Infinity, and Aleph One export
- compatibility report before writing
- checksum and resource handling
- opaque-chunk and invalidation disclosure

Ordinary Save remains Pfhorge-native.

### Productization

- Vulkan and Linux target
- automated corpus regression
- content updates
- document migration and autosave
- CI and signed releases
- accessibility and localization

## Current next step

Implement **CONTENT-1A / VM-SETTINGS-1A** first. The Content Manager establishes
where original and replacement assets come from; the settings layer makes Metal
Visual Mode controllable. `TEX-1A` then consumes those validated profiles to
render original Marathon textures correctly.

## CONTENT-1A / VM-SETTINGS-1A implementation foundation

The next implementation package adds the unified Content Manager, official trilogy distribution installer, local distribution scanning, managed/external content registration, texture-pack import, persistent Visual Mode controls, GPU/display settings, continuous key-state movement, and an optional diagnostics overlay. Classic texture sampling remains TEX-1A.

### CONTENT-1A.1 / VM-SETTINGS-1B — Content Selection and Camera Polish

- simplify content choices to Original, Enhanced, Custom, and Untextured Diagnostic
- activate selected Shapes through VMShapesPath and TextureRepository
- install reviewed M1, M2, and Infinity enhanced recipes after explicit approval
- split mouse axes, inversion, smoothing, and camera tuning
- keep Metal texture sampling in TEX-1A/TEX-1B

### CONTENT-1A.2 / VM-SETTINGS-1B.1 — Content UX, Progress, and High Refresh

- separate required Shapes data from optional enhanced texture profiles
- resolve missing original-data dependencies automatically
- display full resource paths with contextual verify/reveal/copy/remove actions
- stream structured builder progress and improve official download progress
- add a dedicated Content menu
- follow each display's maximum refresh rate, including 240 Hz screens

### TEX-1A — Classic Shapes Textures in Metal

- carry Marathon collection/bitmap descriptors and origin-aware UVs into PreviewScene
- decode selected original Shapes through the existing catalog
- upload classic wall, floor, ceiling, landscape, and media images to Metal
- activate nearest, linear, trilinear, and anisotropic sampler settings
- preserve colored fallback and Untextured Diagnostic mode
- defer enhanced replacement profiles, animated transfer modes, sprites, and composite overlays to later texture phases

### VM-4A / TEX-1A.1 / LEVEL-SYNC-1A

- render temporary platform and door states in Metal Visual Mode
- collision-aware first-person movement and Use/Open Door interaction
- robust wall-side texture resolution with editor-field fallback
- live unsaved level synchronization without save/reopen cycles
- synchronized level-environment texture menus and optional descriptor remap
- active-map Shapes, wall-side, and platform texture audit
