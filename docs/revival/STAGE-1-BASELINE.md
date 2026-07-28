# Stage 1: Baseline and Revival Build

## Objective

Establish a reproducible build of the untouched Pfhorge application before changing behavior or redesigning the interface.

The current project is a native macOS Cocoa application containing Objective-C, Objective-C++, C, and newer Swift sources. The repository's own TODO identifies newer Aleph One formats, endian handling, a broken OpenGL preview, slow serialization, scrolling-related memory growth, deprecated UI elements, and legacy PICT handling as unfinished work.

## Rules for this stage

1. Do not rewrite the map model yet.
2. Do not replace AppKit yet.
3. Do not convert all Objective-C to Swift.
4. Do not alter serialization without a round-trip fixture.
5. Preserve upstream Git history and GPL-2.0 notices.
6. Separate build-system repairs from behavioral fixes.

## Commands

```bash
make -f revival.mk audit
make -f revival.mk baseline
make -f revival.mk stage1
```

Reports are generated in `RevivalArtifacts/`.

## Baseline deliverables

- `source-audit.md`: language inventory and migration-risk indicators.
- `source-audit.json`: complete machine-readable audit.
- `build-environment.txt`: macOS, Xcode, Swift, and Clang versions.
- `xcode-project-list.json`: schemes and targets seen by Xcode.
- `build-settings.txt`: resolved target build settings.
- `xcodebuild.log`: complete build transcript.
- `build-report.md`: summarized result and diagnostics.
- `build-diagnostics.json`: machine-readable compiler diagnostics.

## First build-fix order

Once the baseline transcript exists, fixes should be made in this order:

1. Project and resource references.
2. XIB/interface compilation failures.
3. Removed SDK symbols and header imports.
4. Swift/Objective-C bridging failures.
5. Architecture and integer-width failures.
6. Linker failures.
7. Runtime launch failures.
8. Warnings that indicate map corruption or memory unsafety.
9. Cosmetic warnings.

Each category should be committed separately.

## Stage 1 exit gates

A Stage 1 preview is not complete merely because Xcode produces an `.app`. The following must all pass:

- Application launches on Apple Silicon.
- Blank map creation works.
- Existing map opens and displays geometry.
- Save and reopen succeed.
- Exported map is accepted by Aleph One.
- No known unintended semantic changes occur during a save-only round trip.
- At least one fixture from Marathon 1, Marathon 2, Marathon Infinity, and Aleph One is exercised.
- A multi-level scenario compiles.
- A terminal-bearing map survives round trip.
