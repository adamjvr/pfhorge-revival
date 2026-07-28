# Pfhorge Revival Test Matrix

## Fixture policy

Do not commit proprietary Bungie scenario data unless its redistribution terms clearly permit it. Prefer openly redistributable community fixtures, purpose-built minimal maps, or user-supplied local fixtures excluded from Git.

Each fixture should include provenance, license/permission, expected engine family, and the features it exercises.

## Required fixture families

| Family | Minimum purpose |
|---|---|
| Marathon 1 | M1 polygon and texture semantics, including Ouch/Glue where available |
| Marathon 2 | Standard M2 map container and object behavior |
| Marathon Infinity | Infinity map and terminal behavior |
| Aleph One | Newer extensions and relaxed limits |
| Scenario | Multi-level compilation and level directory handling |
| Terminal | Text, formatting, branches, teleports, and images |
| Platform | Platform flags, tags, switches, and height behavior |
| Media | Liquids, lights, ambient/random sounds |
| Malformed | Defensive parsing and useful diagnostics |

## Round-trip protocol

For every supported fixture:

1. Parse the original.
2. Record a semantic inventory.
3. Save without intentional edits.
4. Reopen the result.
5. Compare semantic inventories.
6. Validate references and bounds.
7. Load the result in the intended Aleph One version.

Binary identity is desirable only where deterministic and appropriate. Semantic identity is the required gate.

## Initial semantic inventory

- level count and names
- point, line, side, and polygon counts
- object counts by type
- floor and ceiling heights
- polygon types and permutation references
- texture descriptors and offsets
- lights, media, ambient sounds, and random sounds
- platforms and tags
- terminal grouping and section counts
- annotations and layers
- map checksum/container metadata
