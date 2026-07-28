# Pfhorge Revival license policy

## Project license

Pfhorge Revival uses **GPL-3.0-or-later**.

The repository contains the complete GNU GPL version 3 text in `LICENSE`.
Original Pfhorge source was licensed under GPL-2.0-or-later, which permits
distribution under GPL version 3. Moving the combined revival distribution to
GPL-3.0-or-later allows compatible reuse of GPL-3.0-or-later Aleph One code.

This is a project policy document, not legal advice.

## Existing source files

- Preserve original copyright notices.
- Preserve existing valid GPL notices.
- Do not mechanically replace `GPL-2.0-or-later` inside inherited upstream
  source merely to make every file look identical.
- Do not replace an original author's copyright with a revival contributor's
  name.
- Add an accurate modification/provenance notice when materially adapting an
  inherited or third-party file.

The repository-level license determines distribution of the combined work.
Individual inherited files may continue to state their original
GPL-2.0-or-later grant.

## New revival-owned files

Place an SPDX line near the top:

```text
SPDX-License-Identifier: GPL-3.0-or-later
```

Examples:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
```

```python
# SPDX-License-Identifier: GPL-3.0-or-later
```

## Aleph One code

Before copying or adapting Aleph One implementation code, record:

- upstream repository
- exact commit SHA
- upstream file path
- Pfhorge destination path
- nature of the adaptation
- upstream copyright notices
- SPDX/license identifier

Do not copy broad directories wholesale. Import the smallest coherent pieces
needed for editor preview behavior.

The first intended reuse scope is:

- portal visibility traversal
- clipping-window logic
- polygon/surface ordering
- floor, ceiling, wall, media, and landscape surface semantics
- transfer-mode and texture-coordinate behavior

Explicitly excluded from the preview library:

- SDL application and window setup
- network play
- gameplay simulation
- monster AI
- weapons and HUD
- audio engine
- film recording
- Lua game runtime
- Aleph One preferences UI

## Dependencies

For every dependency, document:

- project and URL
- exact release or commit
- license identifier
- source-distribution obligations
- whether it is linked, vendored, or used only during development
- platform scope

Metal and MetalKit are Apple system frameworks. A future Vulkan backend should
prefer the system Vulkan loader on Linux/Windows. MoltenVK may be evaluated
later rather than becoming a requirement for the first macOS renderer.

## Assets and fixtures

Do not commit proprietary Bungie scenarios, Shapes, sounds, or terminal art.
Prefer purpose-built fixtures and redistributable community material with
documented permission.
