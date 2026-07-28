# VM-3 and modern presentation foundation

## VM-3

This increment adds renderer-neutral portal and frame types, camera-polygon
lookup, and topological adjacency traversal.

It does **not** yet claim Aleph One-compatible screen-space portal clipping.
The next increment adds projected clipping windows, portal revisit rules, and
height-split wall surfaces.

## Splash screen

The startup splash is programmatic and does not require final artwork.

If an asset named `PfhorgeSplashArtwork` exists, it fills the splash window.
Otherwise a dark procedural placeholder with the Pfhorge title is shown.

The splash appears during application initialization and dismisses after the
existing texture-loading startup work completes.

Recommended final artwork master:

- 1440×840 or larger
- wide 12:7-ish composition
- important text/logo kept away from edges
- PNG in the asset catalog named `PfhorgeSplashArtwork`

## App icon

The current asset catalog leaves the 256px and 512px slots empty. The new icon
pipeline generates all ten required classic macOS icon renditions from one
1024×1024 PNG master.

Run:

```bash
scripts/revival/generate_macos_appicon.sh ~/Desktop/PfhorgeIconMaster.png
```

Do not enlarge a tiny raster and call it a remaster. The final master should be
redrawn or regenerated at 1024×1024, then downsampled by the script.
