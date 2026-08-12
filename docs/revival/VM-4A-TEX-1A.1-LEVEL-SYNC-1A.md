# VM-4A / TEX-1A.1 / LEVEL-SYNC-1A

This corrective phase closes the largest gaps exposed by TEX-1A runtime testing.

## Visual Mode world behavior

- Platform polygons are evaluated from `PhPlatform` minimum/maximum heights and static flags.
- Doors can be opened and closed with the configurable **Use / Open door** key, Space by default.
- Door animation changes only the immutable preview snapshot. It does not mutate the editable map.
- Collision-aware first-person movement rejects solid boundaries and closed door openings and provides simple axis-separated wall sliding.
- Orbit diagnostic mode and an optional collision-disabled free-flight mode remain available.

This is editor preview simulation, not a complete reimplementation of Aleph One gameplay physics, obstruction handling, tag logic, sounds, or monster activation.

## Wall texture correction

Wall construction now resolves a side through all supported editor relationships:

1. the polygon's direct side pointer;
2. the polygon's stored side index;
3. the line's clockwise or counterclockwise side pointer;
4. the line's stored clockwise or counterclockwise side index;
5. a matching side back-reference to both polygon and line.

When an older Pfhorge document has an empty packed shape descriptor but retains the editor-only collection and texture-number fields, those fields are used as a compatibility fallback. The diagnostics overlay reports textured wall segments and unresolved side gaps.

## Live level synchronization

Visual Mode fingerprints unsaved map geometry, polygon heights, texture descriptors and origins, media, sides, lines, player starts, and platform definitions. It polls at a limited cadence and rebuilds the renderer-neutral snapshot when the live editor model changes. Saving and reopening the map is no longer required for ordinary edits.

## Level environment parity

The texture inspector and Visual Mode can follow the level's current environment without rewriting placed descriptors. An optional preference can remap existing classic wall/floor/ceiling/media descriptors—including older editor-only 17...21 collection fields—when the environment changes; it is off by default because that operation edits the map.

## Audit

**Content > Audit Active Map Textures…** reports:

- active Shapes path and level environment;
- polygon, side, platform, and door counts;
- classic descriptor and unique bitmap counts;
- missing/invalid active-Shapes images;
- polygon-edge and wall-producing-edge counts;
- wall-producing edges whose side cannot be resolved;
- wall descriptor coverage and composite/split/transparent references.

Composite secondary overlays, transparent-side overlays, full transfer-mode animation, tag-controlled platform logic, obstruction damage, and sounds remain later work.
