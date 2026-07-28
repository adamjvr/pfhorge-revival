# Aleph One integration ledger

## Purpose

This file records every Aleph One source file copied, translated, or materially
adapted for Pfhorge Revival.

Do not add copied implementation code without updating this ledger.

## Upstream

- Project: Aleph One
- Repository: `Aleph-One-Marathon/alephone`
- Default branch observed during planning: `master`
- License: GPL-3.0-or-later
- License file: `COPYING`

## Approved initial study scope

- `Source_Files/RenderMain/RenderVisTree.*`
- `Source_Files/RenderMain/RenderSortPoly.*`
- `Source_Files/RenderMain/RenderRasterize.*`
- renderer-neutral concepts from `Rasterizer.*`
- texture, light, media, landscape, and transfer-mode semantics required by the
  above

Approval to study a file is not approval to copy it wholesale.

## Import record template

Copy this section for every adapted file:

```text
Date:
Aleph One commit:
Upstream path:
Pfhorge destination:
Upstream authors/notices preserved:
SPDX identifier:
Adaptation summary:
Tests:
```

## Current imports

None. The preview-core foundation added in this phase is independently written
renderer-neutral scaffolding. Aleph One implementation import begins only after
an exact upstream commit is pinned and the first visibility fixture is selected.

## Integration constraints

- no SDL application shell
- no networking
- no gameplay, AI, weapons, or HUD
- no audio engine
- no global Aleph One preference system
- no OpenGL calls in Preview/Core or Preview/Marathon
- no raw pointers from renderer code into mutable Pfhorge editor objects

## Import record: VM-3B visibility invariants

```text
Date: 2026-07-28
Aleph One commit: 4cd8346e1c51dbba48434ccd301d73794f16e086
Upstream path: Source_Files/RenderMain/RenderVisTree.h
Upstream path: Source_Files/RenderMain/RenderVisTree.cpp
Pfhorge destination: Pfhorge Source/Preview/Core/PreviewVisibility.hpp
Upstream authors/notices preserved: Aleph One attribution recorded here; no
  verbatim implementation block was copied.
SPDX identifier: GPL-3.0-or-later
Adaptation summary: independently written floating-point editor traversal using
  camera-polygon roots, transparent transitions, inherited clip windows, and
  distinct polygon revisits for distinct clipping regions. Aleph One globals,
  fixed-point ray casting, automap mutation, object placement, and rasterizer
  state were not imported.
Tests: PreviewVisibilitySmoke.cpp; PreviewPortalClippingSmoke.cpp
```
