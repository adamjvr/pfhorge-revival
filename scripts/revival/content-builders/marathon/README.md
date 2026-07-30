# Marathon 1 Best-Available HD/3D Visual Pack builder

This builder downloads the original current releases for five Marathon 1 visual upgrades and creates:

`Marathon-1-Best-Available-HD-Visual-Pack.zip`

## Important limitation

This output is deliberately **not** called a 100% complete HD pack. Marathon 1 still has no finished, unified CFP-style replacement for every visual category. The builder instead combines the strongest available separate releases:

1. TTEP M1 wall/environment textures at 1024×1024
2. xBR Monsters for M1
3. Tacticus' M1 Weapons Redux
4. 3D Scenery for M1
5. Marathon over Tau Ceti E-I landscape at 8192×3072

Every component remains in its own folder and receives a `CREDITS.md`. The completed archive also contains `README.md`, `ATTRIBUTION.md`, `SOURCES.md`, `RIGHTS-NOTICE.md`, and `MANIFEST.json`.

## Run it

**macOS or Linux:** double-click `build_pack.command`, or run `python3 build_pack.py`.

**Windows:** double-click `build_pack.bat`, or run `py -3 build_pack.py`.

The generated ZIP appears in `output/`. Downloads are cached in `downloads/`.

## Upstream-host fallback

If an upstream host blocks automatic downloading, open the source page named in the error, download the archive manually, rename it to the requested filename, place it in `downloads/`, and rerun the builder.

## Requirements

Python 3.9 or newer. No third-party packages.
