# Pfhorge Revival — Integrated Roadmap

## Direction

Pfhorge Revival is a GPL-3.0-or-later Marathon map editor with a
renderer-neutral preview core, native Metal on macOS, a future Vulkan backend,
universal Marathon-family map intake, classic Mac container recovery,
optional original/HD content profiles, Visual Mode editing, and explicit safe
export.

Doom WAD support is not in scope. Public terminology uses **Marathon map
container**; historical Aleph One `wad_*` names remain only where source
compatibility requires them.

## Architectural principles

1. Map data and visual assets are separate.
2. File contents are authoritative; extensions and Finder types are hints.
3. Container version, map data dialect, and semantic feature support are
   independent classifications.
4. Import creates a new Pfhorge document and never overwrites historical input.
5. Unknown data is accompanied by a preservation/export loss ledger.
6. Optional content downloads happen at runtime, never during compilation.
7. Large third-party assets live outside `Pfhorge.app` and retain upstream
   licensing and attribution.

## Completed foundation

### Application modernization

- modern splash and startup behavior
- support-directory initialization
- current Xcode baseline
- GPL-3.0-or-later revival source conventions
- preliminary branding/icon foundation

### VM-2

- immutable `PreviewScene`
- live editor-to-preview conversion
- native Metal depth rendering
- interactive camera
- renderer-neutral smoke tests
- legacy OpenGL fallback

### VM-3A/VM-3B

- polygon containment and adjacency
- projected portal traversal
- vertical portal clipping
- upper/lower wall splitting
- portal-aware `PreviewFrame`
- first-person camera and whole-map diagnostic mode
- successful live rendering of a newly authored room

Regression requirement: entering Visual Mode and moving the camera must not
mark an otherwise clean document as edited.

## Dependency-ordered phases

### MAP-1A — Universal Marathon Map Intake Foundation

- raw, AppleSingle, AppleDouble, MacBinary, and native-fork intake
- bounds-checked Marathon container reader
- content and dialect classification
- Map Identification UI
- JSON/CSV/Markdown corpus reporting
- extensionless map selection

### MAP-1B — Semantic Import and Real-Map Testing

- convert selected M2/Infinity levels into new Pfhorge documents
- retain provenance, original directory records, indexes, raw chunks, and
  warnings
- exercise 2D editing and Metal Visual Mode with historical maps

### CONTENT-1A — Marathon Content Registry

- register scenario folders, Shapes, Sounds, Physics, maps, MML, plugins, and
  replacement packs
- scan local trilogy/Aleph One installations
- optional official-data and community-pack installation
- pinned hashes, safe extraction, provenance, credits, and rights notices

Profiles:

- Marathon 1 Original / Enhanced Authoring / Full Visual Preview
- Marathon 2 Original / Official HD / CFP Authoring
- Infinity Original / Official HD / CFP Authoring
- Custom Aleph One Scenario

The supplied M1, M2, and Infinity builder scripts become lightweight curated
manifests. Compilation remains offline.

### TEX-1A — Shapes Catalog and Texture Profiles

- canonical collection/bitmap catalog from Shapes
- original-image fallback
- Aleph One MML replacement inventory
- deterministic pack layering and conflict reporting
- texture/collection browser and thumbnails

HD packs are replacement artwork keyed to canonical collection/bitmap IDs;
they are not the authoritative catalog by themselves.

### VM-3C — Real-Map Visibility Hardening

- near-plane portal clipping
- inherited visibility windows
- portal loop protection
- polygon depth ordering
- portal diagnostics
- 5D and moving-platform validation using imported maps

### TEX-1B — Textured Metal Visual Mode

- base, glow, detail/offset, opacity, landscape, and liquid materials
- mipmaps, sRGB, wrapping, caching, and memory budgeting
- correct wall/floor/ceiling texture coordinates and map offsets

### TEX-2 — Visual Mode Texture Editing

- surface picking
- wall/floor/ceiling painting
- eyedropper and palettes
- alignment and offset tools
- primary/secondary/transparent layers
- lighting and transfer-mode editing
- undo/redo through live document data

### MAP-2 — Full Historical Dialect Coverage

- Marathon 1 and early container version 1
- Marathon 2 and Infinity
- known Aleph One extensions
- historically accepted noncanonical records

### MAP-3 — Overlays and Scenario Dependencies

- parent checksum resolution
- overlay application
- resource forks and external content
- scenario folders, scripts, Shapes, Sounds, and Physics dependencies

### ENTITY-1 — Object and Sprite Preview

- scenery, items, weapons, monsters, players, animation sequences, color
  tables, and optional 3D model replacements

This is where the non-environment portions of the supplied HD builders become
editor-preview inputs.

### EXPORT-1 — Explicit Safe Marathon Export

- target-specific M1/M2/Infinity/Aleph One export
- compatibility report before writing
- checksum/resource handling
- opaque-chunk and invalidation disclosure

Ordinary Save remains Pfhorge-native.

### Scenario and archive intake

- complete scenario directories
- safe ZIP staging
- traversal, symlink, case-collision, file-count, and decompression limits

### Productization

- Vulkan/Linux target
- automated corpus regression
- content manager and updates
- document migration, autosave, CI, signed releases, accessibility, and
  localization

## Current next step

**MAP-1A** is active. It unlocks real historical maps for every later renderer,
content, texture, object, and export test.
