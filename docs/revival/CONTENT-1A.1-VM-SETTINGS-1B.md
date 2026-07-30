# CONTENT-1A.1 / VM-SETTINGS-1B — Content Selection and Camera Polish

## Goal

Finish the user-facing content-selection bridge and refine the already working
Metal Visual Mode controls before textured Metal rendering begins.

This phase does **not** claim that Metal samples Shapes or HD images yet. That
remains TEX-1A and TEX-1B. This phase makes the sources installable,
understandable, persistent, and consumable by those renderer phases.

## Content Manager model

Each game presents three ordinary choices:

1. **Original** — official or user-selected Shapes and distribution assets.
2. **Enhanced** — a selected replacement profile layered over Original.
3. **Custom** — scenario-specific Shapes and plugins selected by the user.

`Untextured Diagnostic` remains available as an explicit renderer fallback.

Enhanced profiles never replace the canonical Shapes catalog. Missing enhanced
artwork falls back to the active original Shapes source.

## Original distributions

The official installer resolves the exact data-archive name from the latest
Aleph One release tag:

```text
Marathon-<release>-Data.zip
Marathon2-<release>-Data.zip
MarathonInfinity-<release>-Data.zip
```

The selected archive must include a published SHA-256 digest. Downloads are
staged, verified, audited, and only then promoted into managed storage.

The selected game row is preserved while the sidebar refreshes, preventing a
Marathon 2 or Infinity action from silently falling back to Marathon 1.

## Shapes activation bridge

Selecting Original, Enhanced, or Custom now:

- chooses a preferred Shapes candidate;
- writes the existing `VMShapesPath` preference;
- calls `TextureRepository.loadAllTextures()` through its Objective-C bridge;
- asks the application delegate to rebuild texture menus;
- records the active game, Shapes path, and enhanced-profile path;
- restores the selection on later launches.

This lets the existing 2D texture browser and legacy texture consumers use
Content Manager installations immediately. Metal surface texturing still waits
for TEX-1A.

## Recommended enhanced profiles

The reviewed builder sources supplied for this project are installed as
first-class recipes:

- Marathon 1 Best-Available HD/3D
- Marathon 2 CFP Complete HD
- Marathon Infinity CFP Complete HD

Pfhorge explains the components and asks for approval before executing a
builder. The builder uses Python 3, downloads upstream packages into a reusable
cache, preserves rights and attribution files, generates a ZIP, and passes that
ZIP through the same guarded extraction and scanning pipeline as manual imports.

Builder failures preserve the complete log and cache. Some upstream hosts may
require the user to place a manually downloaded archive into the displayed
cache and retry.

## Visual Mode settings

Settings are divided into:

- Key Bindings
- Mouse Look
- Camera
- Display & GPU
- Textures & Content
- Diagnostics

Mouse Look now has independent horizontal and vertical sensitivity, independent
horizontal and vertical inversion, and optional delta smoothing. Default
horizontal look is corrected so dragging right turns right.

Camera settings now include horizontal movement speed, vertical movement
multiplier, field of view, and near clipping distance.

## Acceptance gate

- Apply closes the settings window.
- WASD and Q/E remain continuous and frame-time based.
- rightward mouse drag turns right by default.
- horizontal and vertical inversion work independently.
- sensitivity and smoothing update live.
- official Original installation targets the selected M1, M2, or Infinity row.
- selected Shapes writes `VMShapesPath` and reloads texture collections.
- all three recommended enhanced recipes appear by name.
- enhanced builders run only after explicit approval.
- generated packs are safely extracted and registered.
- Original and Enhanced selections survive application restart.
- Visual Mode remains untextured until TEX-1A; no false completion claim is made.
