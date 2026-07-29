# MAP-1A — Universal Marathon Map Intake Foundation

## Goal

Allow Pfhorge to inspect extensionless and classic-Mac-packaged Marathon map
files before conversion into mutable editor objects.

MAP-1A deliberately separates:

1. source envelope decoding,
2. Marathon container parsing,
3. content classification,
4. semantic map conversion,
5. content/texture resolution.

The first four no longer depend on filename extensions or Finder metadata.

## Supported source envelopes

- raw data fork
- AppleSingle
- AppleDouble main file plus `._` sidecar
- direct selection of an AppleDouble sidecar
- MacBinary
- native macOS resource fork discovery for raw files

AppleDouble resources are detected and reported. Importing every resource from
an AppleDouble or MacBinary resource fork remains a later resource-preservation
step; MAP-1A prioritizes safe geometry intake.

## Marathon container inspection

The reader records:

- container version and data version independently
- original 64-byte internal-name field
- stored and computed CRC-32
- parent checksum
- directory offset and entry count
- directory ordinal and logical entry index
- application-specific directory bytes
- entry and directory record sizes
- ordered chunk/tag inventory
- structural and compatibility findings

A container version is not treated as a complete game identity. Data dialect
classification also considers `data_version` and the map chunk inventory.

## Safety rules

Untrusted map contents must not reach assertions or unchecked allocation.
The probe rejects or reports:

- arithmetic overflow
- negative lengths
- out-of-file offsets
- malformed directory sizes
- invalid or cyclic tag chains
- unsupported structure sizes
- missing geometry chunk sets

A checksum mismatch is a warning, not automatic corruption.

## UI behavior

`Import Marathon Map…` permits extensionless files. After selection Pfhorge
shows a Map Identification dialog containing:

- detected dialect
- source envelope
- container and data versions
- entry count
- internal name
- classic Finder type, when available
- checksum status
- parent checksum
- resource-fork status
- level names available in directory metadata
- chunk inventory
- warnings and compatibility findings

Only a structurally usable map is passed to the existing `LEMapData` semantic
converter. The source file is never modified.

## Corpus probe

Run against a directory, individual file, or ZIP archive:

```bash
python3 scripts/revival/probe_map_corpus.py \
  /path/to/Marathon_Map_Files_Only.zip \
  --output-dir RevivalArtifacts
```

Outputs:

- `map-corpus-report.json`
- `map-corpus-report.csv`
- `map-corpus-summary.md`

## Validation

```bash
make -f revival.mk map-intake-check
```

To include a local corpus scan:

```bash
PFHORGE_MAP_CORPUS=/path/to/maps.zip \
make -f revival.mk map-intake-check
```

## Deferred to MAP-1B and later

- full semantic conversion for every historical dialect quirk
- resource extraction from every outer container
- overlay application and parent-map discovery
- unknown-chunk round-trip guarantees
- archive extraction UI
- Shapes/content profile acquisition
- texture rendering and Visual Mode painting
