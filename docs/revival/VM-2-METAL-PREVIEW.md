# VM-2: read-only Metal geometry preview

## Status

This milestone adds an opt-in Metal preview beside the legacy OpenGL Visual
Mode. The legacy mode remains the default and is not deleted.

VM-2 renders the currently open `LELevelData` as:

- polygon floors
- polygon ceilings
- wall quads
- depth-tested untextured geometry
- stable per-polygon diagnostic colors

It also provides:

- Retina-aware `MTKView` resizing
- mouse-drag orbit
- scroll-wheel zoom
- `W`, `A`, `S`, `D` target movement
- `Q` and `E` vertical movement
- `R` camera reset
- automatic framing of the current level

## Enabling the prototype

### Xcode scheme

Add this environment variable to the Pfhorge Run scheme:

```text
PFHORGE_METAL_PREVIEW=1
```

Then launch Pfhorge, open or create a level, and invoke the existing Visual Mode
command.

### Persistent user default

The bundle identifier currently used by the project is `Pfhorge`.

```bash
defaults write Pfhorge PfhorgeUseMetalPreview -bool YES
```

Disable it with:

```bash
defaults delete Pfhorge PfhorgeUseMetalPreview
```

If Metal initialization or shader compilation fails, the controller logs the
failure and falls back to the legacy OpenGL view.

## Intentional limitations

VM-2 is an integration and geometry milestone, not Forge parity.

- All polygons are emitted without portal visibility.
- Interior shared walls may be duplicated.
- Textures are not decoded or sampled.
- Lighting is represented only by diagnostic colors.
- Concave polygons currently use a triangle fan and are diagnostic-only.
- Landscapes, media, sprites, transfer modes, platforms, and object rendering
  are not implemented.
- The preview is read-only.
- Picking is not implemented.

These limitations are removed in later milestones rather than patched into the
prototype with unrelated heuristics.

## Validation checklist

1. Run `make -f revival.mk preview-core-check`.
2. Run `make -f revival.mk baseline`.
3. Launch the application normally and confirm legacy Visual Mode still opens.
4. Enable `PFHORGE_METAL_PREVIEW=1`.
5. Open a simple rectangular map.
6. Invoke Visual Mode.
7. Confirm floor, ceiling, and walls render.
8. Resize the window and verify the aspect ratio remains correct.
9. Test orbit, zoom, movement, and reset.
10. Open a more complex map and record any obviously malformed polygons.
11. Disable the feature flag and confirm the OpenGL fallback remains intact.

## VM-3 handoff

The next milestone replaces whole-level surface emission with verified Marathon
portal visibility:

```text
camera polygon
    -> portal traversal
    -> clipping windows
    -> visible polygon ordering
    -> renderer-neutral PreviewFrame
    -> Metal draw
```

Before adapting Aleph One implementation code, pin an exact upstream commit in
`docs/revival/ALEPH-ONE-INTEGRATION.md`.
