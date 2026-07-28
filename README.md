# Pfhorge Revival

A maintained continuation of **Pfhorge**, the native macOS map editor for classic
Marathon and Aleph One.

This repository begins by restoring and validating the surviving Objective-C,
Objective-C++, C, and Swift application before replacing major subsystems. The
project plan is deliberately staged:

1. Restore a reproducible modern macOS build.
2. Modernize the existing editor without changing map behavior.
3. Extract and test a UI-independent map-format core.
4. Build the eventual cross-platform successor from verified behavior.

## Stage 1: establish the baseline

Pfhorge requires macOS and Xcode to compile. From the repository root:

```bash
./scripts/revival/bootstrap_macos.sh --no-branch
```

Or run the individual targets:

```bash
make -f revival.mk audit
make -f revival.mk baseline
make -f revival.mk stage1
```

Generated reports are written to `RevivalArtifacts/` and intentionally ignored
by Git. Archive that directory after the first Mac build so compiler and linker
failures can be converted into the first source-fix commits.

Documentation:

- [`docs/revival/STAGE-1-BASELINE.md`](docs/revival/STAGE-1-BASELINE.md)
- [`docs/revival/TEST-MATRIX.md`](docs/revival/TEST-MATRIX.md)
- [`docs/revival/LICENSE-POLICY.md`](docs/revival/LICENSE-POLICY.md)
- [`docs/revival/FIRST-MAC-TEST.md`](docs/revival/FIRST-MAC-TEST.md)

## Source provenance

Pfhorge was created by Joshua D. Orr and subsequently maintained and modernized
by other contributors. Preserve all existing copyright and license notices in
upstream files. See [`NOTICE.md`](NOTICE.md) for the revival repository's
attribution policy.

## License

Pfhorge Revival is distributed under the **GNU General Public License, version
2 or, at your option, any later version**.

SPDX identifier: `GPL-2.0-or-later`

See [`LICENSE`](LICENSE).
