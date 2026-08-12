# TEX-1A — Classic Shapes Textures in Metal

## Purpose

TEX-1A turns the selected original Marathon Shapes file into a real texture
source for native Metal Visual Mode. It deliberately builds on the immutable
PreviewScene boundary instead of allowing the renderer to retain editor-model
objects.

## Implemented

- Stable Objective-C image lookup in `TextureRepository` by Shapes collection
  and bitmap index.
- Support for Pfhorge-normalized collections `0...4` and `10...13`, plus raw
  Marathon collections `17...21` and `27...30`.
- PreviewScene revision 4 with texture descriptors on floors, ceilings, wall
  segments, landscapes, and polygon media surfaces.
- Floor and ceiling UVs relative to their Marathon texture origins.
- Wall UVs derived from world-space length, height, and side texture offsets.
- Split sides use the secondary texture for the lower segment and the primary
  texture for the upper segment.
- Landscape-marked sides use their classic landscape bitmap as a full-image
  surface.
- Classic NSImage data is uploaded through `MTKTextureLoader`, cached by
  collection/bitmap, and rendered with generated mipmaps.
- Nearest, linear, and trilinear filtering plus anisotropy now control the
  Metal sampler.
- Media is rendered in a translucent pass with depth testing but no depth
  writes.
- Missing or invalid images retain the colored diagnostic fallback.
- `Untextured Diagnostic` remains a forced colored-rendering mode.
- Content changes clear the Metal cache and rebuild visible draw packets.
- The diagnostics overlay reports textured surfaces, fallback surfaces, and
  resident classic texture count.

## Deliberately deferred

- Enhanced replacement images and profile overlays: TEX-1B.
- Animated transfer modes, texture scrolling, pulsing, and wobble.
- Composite-side secondary overlays and transparent portal-side overlays.
- Object, monster, weapon, and scenery sprites.
- Lighting-state evaluation beyond the existing renderer-neutral vertex
  brightness field.
- Perspective-correct panoramic landscape projection beyond basic full-image
  placement.

## Acceptance

1. A map with valid selected Shapes displays classic floor, ceiling, and wall
   images in Metal Visual Mode.
2. Landscape and media surfaces display a classic image when their descriptors
   are valid.
3. Missing bitmap references remain visible in diagnostic colors rather than
   crashing or disappearing.
4. Original and Enhanced content selections both retain original Shapes as the
   TEX-1A fallback catalog; enhanced replacement rendering remains deferred.
5. Nearest, linear, and trilinear settings visibly change sampling.
6. `Untextured Diagnostic` forces the prior colored renderer.
7. Changing the active game or Shapes file invalidates the Metal cache.
8. Portal visibility, mouse look, WASD/QE, 240 Hz handling, diagnostics, and
   map-document dirty state remain unchanged.
