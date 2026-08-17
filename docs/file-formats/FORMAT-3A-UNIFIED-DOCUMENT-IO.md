# FORMAT-3A — Unified Document I/O and Native-Only Level Persistence

## Persistence policy

Pfhorge-authored level persistence is now one-way toward the current native format:

```text
New level                 -> Pfhorge Native .pfhlev
Save level                -> Pfhorge Native .pfhlev
Scenario-imported level   -> Pfhorge Native .pfhlev
Pathways-converted level  -> Pfhorge Native .pfhlev
Legacy Pfhorge migration  -> Pfhorge Native .pfhlev
```

External formats remain interchange sources/targets:

```text
Marathon / Aleph map      -> readable source + explicit export target
Legacy Pfhorge .pfhlev    -> readable migration source only
Pathways                  -> readable/conversion source
```

There is no separate user-facing "new Pfhorge" file type. `.pfhlev` remains the
Pfhorge Level extension. Old and new generations are distinguished by their bytes.

## Why the overhaul reaches beyond the serializer

The historical program routes formats independently through:

- Info.plist document declarations;
- `NSDocumentController` Open-panel filtering;
- `LEMap` raw Marathon fallback;
- `PhPfhorgeSingleLevelDoc`;
- Scenario Marathon Import;
- Scenario Pathways Import;
- scenario merged-Marathon export.

Runtime testing showed why that is unsustainable: extensionless Marathon sources were
disabled by File → Open, while Scenario Import continued writing legacy `.pfhlev`
files even after the single-level document writer had moved to the native package.

FORMAT-3A unifies those active level paths.

## File → Open

`PfhorgeUnifiedDocumentController` is instantiated before `NSApplicationMain`, making
it the shared AppKit document controller.

Its Open panel does not disable files merely because their modern filename extension
is unknown. Classic Marathon assets may be extensionless or identified by old HFS
metadata.

Type routing checks:

1. `.pfhlev`
2. `.sen`
3. known Marathon extensions
4. classic HFS type codes
5. legacy Pfhorge header signature
6. AppKit's normal type resolver
7. otherwise, a regular file is offered to the Marathon source reader

The final fallback does **not** assert that arbitrary files are Marathon maps. It only
moves structural validation out of the filename filter and into the actual map reader.

## Direct-open Marathon files

The Marathon document declaration routes to `PhPfhorgeSingleLevelDoc`.

Direct Open uses the same modern source-envelope resolver as Scenario Import, including
AppleDouble/AppleSingle/MacBinary/resource-fork handling. A multi-level WAD presents a
single-level chooser because File → Open creates one editable level document; Scenario
Import remains the multi-level workflow.

The original Marathon source is never a Pfhorge persistence target. In this transition
`PhPfhorgeSingleLevelDoc +autosavesInPlace` returns `NO`, and the existing
`cameFromMarathonFormatedFile` Save behavior converts the first normal Save to a
Pfhorge `.pfhlev` destination.

## Scenario Marathon Import

`LEMapData` gains a direct semantic API:

```objc
+convertMarathonDataToLevels:levelNames:error:
```

The active scenario path no longer serializes through historical Pfhorge bytes:

```text
source resolver
    ↓
Marathon structural probe
    ↓
LEMapData parser
    ↓
LELevelData[]
    ↓
Pfhorge Native package writer
    ↓
scenario/*.pfhlev
```

The older `convertMarathonDataToArchived:` method may remain temporarily for source
compatibility but is no longer the active Marathon Scenario Import path.

## Pathways transition

The current Pathways converter still emits archived Pfhorge-level bytes. FORMAT-3A
treats those as an **in-memory migration artifact only**:

```text
Pathways converter
    ↓
legacy in-memory bytes
    ↓
legacy migration decoder
    ↓
LELevelData
    ↓
native writer
```

Those historical bytes are never written as scenario level files.

## Scenario merged-Marathon export

The old merger assumed every `.pfhlev` had a ten-byte historical header and directly
unarchived everything after byte ten. That breaks once scenario levels are native ZIPs.

FORMAT-3A replaces that assumption with `PfhorgeLevelFromAnyPfhlevData()`, which reads:

- current native FORMAT-2A packages;
- historical `.pfhlev` files for migration.

This intentionally permits mixed old/new scenario directories while users migrate.

## Transitional limitations

FORMAT-3A still inherits FORMAT-2A's required snapshot-authority bridge inside new
native packages. Routing and physical persistence are the focus here. Direct canonical
JSON → `LELevelData` reconstruction is the following native-codec milestone.

Scenario `.sen` metadata persistence is also still historical. Scenario-container
migration remains a separate overhaul after level I/O is unified.
