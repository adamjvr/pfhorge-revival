# MAP-1A Format Sources and Provenance

MAP-1A is independently implemented for Pfhorge under GPL-3.0-or-later. The
following primary references define the structures and compatibility behavior
used by the reader.

## Marathon container format

Pinned Aleph One revision studied:

```text
4cd8346e1c51dbba48434ccd301d73794f16e086
```

Relevant files:

- `Source_Files/Files/wad.h`
  - 128-byte header
  - old and current directory records
  - old and current entry headers
  - container versions 0, 1, 2, and 4
- `Source_Files/Files/wad.cpp`
  - version-dependent record sizes
  - explicit logical entry indexes
  - tag-chain traversal
  - checksum behavior
- `Source_Files/GameWorld/map.h`
  - 74-byte map directory metadata record
  - mission flags, environment flags, entry-point flags, and level name
- `Source_Files/Files/game_wad.cpp`
  - separation between container version and `data_version`
  - old/new level-name and entry-point discovery

Repository:

```text
https://github.com/Aleph-One-Marathon/alephone
```

## AppleSingle and AppleDouble

RFC 1740, *MIME Encapsulation of Macintosh Files — MacMIME*:

```text
https://www.rfc-editor.org/rfc/rfc1740
```

Used details:

- AppleSingle magic `0x00051600`
- AppleDouble magic `0x00051607`
- big-endian header and entry descriptors
- data fork entry ID 1
- resource fork entry ID 2
- Finder information entry ID 9
- AppleDouble sidecars normally omit the data fork
- entry descriptors may appear in any order

## MacBinary

MacBinary II standard proposal and compatible archival descriptions were used
for:

- 128-byte header
- data-fork length at offset 83
- resource-fork length at offset 87
- secondary-header length at offset 120
- 128-byte fork alignment

The implementation validates the complete computed fork ranges before exposing
either fork.

## Checksum

Aleph One uses reflected CRC-32 polynomial `0xEDB88320`, initialized and
finalized with `0xFFFFFFFF`. The checksum field is treated as zero while the
file checksum is calculated.
