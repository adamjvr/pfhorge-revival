# Marathon Infinity CFP Complete HD Pack builder

This small builder downloads the current original Community/Freeverse Plugin component archives and creates:

`Marathon-Infinity-CFP-Complete-HD.zip`

The generated pack keeps walls, weapons, monsters, and scenery in separate folders, and adds:

- `README.md`
- `ATTRIBUTION.md`
- one `CREDITS.md` inside every component folder
- `SOURCES.md`
- `MANIFEST.json` with SHA-256 hashes
- `LICENSE-GPL-3.0.txt`

## Run it

**macOS or Linux:** double-click `build_pack.command`, or run `python3 build_pack.py`.

**Windows:** double-click `build_pack.bat`, or run `py -3 build_pack.py`.

The completed ZIP appears in the `output` folder. Downloaded upstream archives are cached in `downloads`, so a retry does not re-download successful components.

## Google Drive fallback

The current releases are hosted through Google Drive. The builder handles the normal confirmation flow automatically. If Google rate-limits a file, the error message gives its source page and exact filename. Download that archive in a browser into the local `downloads` folder, then rerun the builder.

## Requirements

Python 3.9 or newer. No third-party Python packages are required.

## Upstream

- Source repository: https://github.com/JoshuaPettus/Marathon-Community-Freeverse-Textures
- License supplied by upstream: GNU GPL v3.0
- Builder prepared: 2026-07-28
