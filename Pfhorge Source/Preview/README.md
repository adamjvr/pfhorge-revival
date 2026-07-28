# Pfhorge Preview subsystem

This directory contains the replacement Visual Mode architecture.

- `Core/` is renderer-neutral and must compile without AppKit, Metal, Vulkan,
  OpenGL, or SDL.
- `Marathon/` will contain attributed Marathon visibility and surface semantics.
- `Metal/` will contain the first production backend.
- `Vulkan/` is reserved for the cross-platform stage.
- `Tests/` will contain visibility, surface, picking, and image regressions.

The core must never retain raw pointers to mutable Objective-C editor objects.
`PreviewSceneBuilder.mm` will create immutable snapshots carrying stable IDs.
