# Pfhorge Revival

A maintained continuation of **Pfhorge**, the native macOS map editor for classic
Marathon and Aleph One.

Pfhorge Revival now has a reproducible Apple Silicon build. Development proceeds
in compatibility-first stages so modernization does not silently alter map
semantics.

## Development stages

1. **Baseline restoration — complete**
   - reproducible macOS/Xcode build
   - Apple Silicon launch
   - baseline audit and diagnostic capture
2. **Behavioral validation and Forge parity — active**
   - fixture corpus
   - semantic save/reopen tests
   - scenario, terminal, platform, light, liquid, and sound validation
3. **Visual Mode revival — active**
   - renderer-neutral preview scene
   - Aleph One-derived portal visibility and Marathon surface semantics
   - native Metal renderer through `MTKView`
   - GPU picking and Forge-compatible texture editing
4. **Map core extraction**
   - UI-independent parsing, validation, serialization, and semantic inventory
5. **Modern editor**
   - replace deprecated AppKit controls without changing behavior
   - improve diagnostics, performance, undo, and accessibility
6. **Cross-platform successor**
   - Vulkan backend for Linux and Windows
   - shared preview and map core
   - optional MoltenVK path on Apple platforms

## Build

Pfhorge requires macOS and a full Xcode installation.

```bash
./scripts/revival/bootstrap_macos.sh --no-branch
```

Individual targets:

```bash
make -f revival.mk audit
make -f revival.mk baseline
make -f revival.mk stage1
make -f revival.mk preview-core-check
```

Generated build reports are written to `RevivalArtifacts/` and are intentionally
ignored by Git.

## Visual Mode architecture

The revived Visual Mode will not embed the complete Aleph One game. It will use
a small, attributed compatibility layer for Marathon-specific visibility,
surface construction, texture coordinates, transfer modes, lighting, liquids,
landscapes, and sprites.

```text
Pfhorge mutable editor model
        |
        v
Immutable PreviewScene snapshot
        |
        v
Marathon portal visibility and surface builder
        |
        v
Renderer-neutral draw packets
        |
        +----> Metal backend on macOS
        |
        +----> Vulkan backend in the cross-platform stage
```

OpenGL remains a temporary behavioral reference only. New renderer work targets
Metal first because Pfhorge is currently a native AppKit application.

See:

- [`docs/revival/VISUAL-MODE-ROADMAP.md`](docs/revival/VISUAL-MODE-ROADMAP.md)
- [`docs/revival/ALEPH-ONE-INTEGRATION.md`](docs/revival/ALEPH-ONE-INTEGRATION.md)
- [`docs/revival/FORGE-PARITY-MATRIX.md`](docs/revival/FORGE-PARITY-MATRIX.md)
- [`docs/revival/TEST-MATRIX.md`](docs/revival/TEST-MATRIX.md)
- [`docs/revival/LICENSE-POLICY.md`](docs/revival/LICENSE-POLICY.md)

## Source provenance

Pfhorge was created by Joshua D. Orr and subsequently maintained and modernized
by other contributors. Preserve all existing copyright and license notices in
inherited files. See [`NOTICE.md`](NOTICE.md).

## License

Pfhorge Revival is distributed under the **GNU General Public License,
version 3 or later**.

SPDX identifier: `GPL-3.0-or-later`

See [`LICENSE`](LICENSE).
