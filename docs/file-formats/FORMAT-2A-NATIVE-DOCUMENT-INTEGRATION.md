# FORMAT-2A — Native Document Transport Integration

## Milestone

FORMAT-2A crosses the application boundary: `PhPfhorgeSingleLevelDoc` now recognizes and
writes the new Pfhorge Native ZIP container.

The historical reader is retained, so old files still open:

```text
old .pfhlev
10-byte Pfhorge header + NSKeyedArchive
        |
        v
existing legacy reader
```

New Pfhorge saves use:

```text
new .pfhlev
ZIP package
├── mimetype
├── manifest.json
├── document.json
├── levels/<uuid>.json
└── bridge/level.archive
```

The file still has the `.pfhlev` extension and the existing Pfhorge Level UTI.

## Transitional authority rule

FORMAT-2A does **not** claim that an incomplete canonical mirror is authoritative.

Its manifest declares:

```text
org.pfhorge.format2a.snapshot-authority
requiredForRead  = true
requiredForWrite = true
```

A reader that does not understand that required extension must refuse the package rather
than silently interpret the placeholder canonical arrays as an empty level.

The bridge resource is a secure `NSKeyedArchiver` snapshot of `LELevelData`. It is the
authoritative payload for this milestone.

`level.json` is still useful and schema-valid. It contains:

- the stable level UUID;
- level name;
- environment / physics / song / mission / environment / entry flags;
- exact object-domain counts;
- a loud `canonicalAuthority: false` extension marker.

FORMAT-2B replaces bridge-backed reconstruction with direct Canonical JSON ->
`LELevelData` reconstruction. Only then does the canonical semantic object graph become
authoritative and this required extension disappears.

## Why stage it this way

Changing the physical document format, Cocoa document dispatch, package probing,
ZIP handling, manifest/resource validation, and the canonical graph reconstruction all
at once would make failures difficult to isolate.

FORMAT-2A gives us a runtime-testable checkpoint:

1. legacy file opens;
2. Save migrates it to a ZIP-backed `.pfhlev`;
3. the new package closes/reopens;
4. the map is unchanged because the complete old graph is preserved;
5. Finder/document type remains `.pfhlev`;
6. package internals are inspectable with `unzip`.

That isolates package/document integration before FORMAT-2B takes over semantic
reconstruction.

## Package rules implemented by the Cocoa bridge

- ZIP Store writer.
- `mimetype` is first and uncompressed.
- deterministic ZIP timestamps are zero.
- UTF-8 JSON with sorted keys where available.
- SHA-256 manifest resource hashes.
- path traversal/backslash/duplicate path rejection.
- encrypted/unsupported-compression rejection.
- ZIP CRC and size checks.
- package entry count / resource-size bounds.
- exact Pfhorge mimetype validation.
- exact required FORMAT-2A bridge extension validation.

The FORMAT-2A Cocoa reader accepts Store packages emitted by Pfhorge itself. Deflate
support remains in the Python reference reader and is promoted into the Cocoa reader in
FORMAT-2B; a required bridge package repacked with Deflate will be rejected by this
milestone rather than guessed at.

## Save behavior

For `PhPfhorgeSingleLevelDoc`:

```text
Marathon target -> existing Marathon writer
Pfhorge target  -> FORMAT-2A native package writer
```

So an old `.pfhlev` naturally migrates on Save while retaining the same external file
type.

## Read behavior

Native ZIP probing occurs before the old code touches the historical 10-byte header:

```text
PK... + Pfhorge manifest -> FORMAT-2A package reader
legacy Pfhorge signature -> existing legacy reader
```

## Acceptance test

A passing `format2a-check` proves compilation and contract tests only.

Runtime acceptance requires:

1. Open a copy of a real legacy `.pfhlev`.
2. Save.
3. Confirm the saved file begins with `PK`.
4. Inspect with `unzip -l`.
5. Close and reopen it.
6. Exercise 2D editor + Visual Mode + terminals/layers/media.
7. Compare against the pre-migration copy.

Do not commit FORMAT-2A until that runtime round-trip is accepted.
