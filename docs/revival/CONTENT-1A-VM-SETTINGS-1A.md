# CONTENT-1A / VM-SETTINGS-1A — Distribution-Aware Content Manager

## Goal

Provide one unified macOS Content Manager for original Marathon distribution
assets, Shapes files, embedded original textures, external replacement
textures, Aleph One plugins, and optional HD profiles. At the same time, replace
the Metal Visual Mode's hardcoded controls with persistent, rebindable settings.

## Why this phase comes next

Historical map intake and first-person portal rendering are now working well
enough to expose the next dependency: Visual Mode needs a reliable content
source before textured rendering can be correct.

Implementing the renderer before the content registry would hardcode paths and
make original, enhanced, and custom scenario content difficult to distinguish.

## User-facing entry points

```text
Pfhorge > Content Manager…
View > Visual Mode & GPU Settings…
```

The GPU settings window links directly to the Content Manager.

When a map requires content that is not configured:

```text
Marathon 2 content is not installed.

[Install Content…] [Use Untextured Mode] [Cancel]
```

## Content Manager game cards

Provide cards for:

- Classic Marathon
- Marathon 2: Durandal
- Marathon Infinity
- Custom Scenarios

Each card reports:

- selected distribution
- source type: managed, external, or missing
- Shapes source and checksum
- detected embedded original collections
- bundled plugins and external texture files
- active texture profile
- validation status
- available actions

Actions:

```text
Install Official Distribution…
Scan This Mac…
Choose Existing…
Use in Place
Copy into Pfhorge
Verify
Repair
Rescan
Reveal in Finder
View Manifest
View License and Credits
Remove Managed Copy
```

## Distribution scanning

A distribution is scanned as a complete content source.

Recognize:

- Shapes and Shapes-family files
- external PNG, JPEG, DDS, and other supported textures
- MML files
- Aleph One plugin directories
- plugin ZIP archives
- landscapes
- glow and normal maps
- scenario-specific Shapes
- license and credit files

The scanner must not assume one Shapes file per game.

## Content profiles

A profile identifies:

- game or custom scenario
- base Shapes source
- enabled bundled plugins
- enabled external HD profile
- project-local overrides
- fallback policy
- provenance manifest

Built-in profile modes:

```text
Original
Distribution Default
Enhanced
Custom
Untextured Diagnostic
```

Original uses textures embedded in Shapes only.

Distribution Default uses Shapes plus the compatible plugins normally enabled
by that distribution.

Enhanced adds a selected HD profile.

Custom allows explicit Shapes and plugin combinations.

## Safety

Treat downloaded and imported archives as untrusted.

Require:

- explicit user approval before network access
- HTTPS source allowlist or signed/pinned manifest
- SHA-256 verification
- temporary staging directory
- path traversal rejection
- absolute-path rejection
- symlink and hard-link rejection
- case-collision detection
- file-count and expanded-size limits
- atomic promotion after validation
- rollback on failure
- no mutation of external installations

## Provenance manifest

Record:

```json
{
  "schemaVersion": 1,
  "displayName": "Classic Marathon 2",
  "game": "marathon2",
  "sourceType": "officialDownload",
  "sourceURL": "...",
  "packageVersion": "...",
  "archiveSHA256": "...",
  "installedAt": "...",
  "shapes": [
    {
      "originalPath": "Shapes.shpA",
      "managedPath": "Shapes/Marathon2/Shapes.shpA",
      "sha256": "..."
    }
  ],
  "plugins": [],
  "licenses": [],
  "validation": {
    "status": "valid",
    "findings": []
  }
}
```

## Visual Mode settings

Create a renderer-facing settings snapshot. The Metal view should not read UI
controls directly.

Required default bindings:

| Action | Default |
|---|---|
| Forward | W |
| Backward | S |
| Strafe left | A |
| Strafe right | D |
| Fly down | Q |
| Fly up | E |
| Reset camera | R |
| Toggle orbit diagnostic | P |
| Diagnostics | I |

Mouse:

- drag to look in first-person
- drag to orbit in diagnostic mode
- configurable sensitivity
- configurable invert Y
- scroll changes movement speed in first-person
- scroll changes orbit distance in diagnostic mode

Replace one-step key-down movement with continuous pressed-key state and
frame-time-based movement.

## GPU and rendering settings

- preferred Metal device
- frame-rate limit: 30, 60, 120, display, unlimited
- VSync
- render scale
- MSAA: off, 2x, 4x, 8x when supported
- field of view
- near-plane distance
- texture filtering
- anisotropic filtering
- back-face culling
- untextured diagnostic mode

Unsupported settings should be disabled with an explanation rather than silently
ignored.

## Diagnostics settings

- FPS
- frame time
- camera position
- camera and seed polygon IDs
- visible polygon and surface counts
- portals examined and accepted
- texture collection and bitmap IDs
- missing texture warnings
- optional portal outlines
- copy diagnostic report

The `I` key remains available and prints or copies the same underlying
diagnostic snapshot.

## Implementation boundaries

CONTENT-1A catalogs and installs content; it does not yet require final textured
Metal rendering.

TEX-1A consumes the validated profiles and adds `MTLTexture` rendering.

The current colored renderer remains the fallback throughout this phase.

## Validation

Automated:

- settings serialization and migration tests
- binding conflict tests
- content-manifest parsing tests
- safe archive extraction tests
- distribution scanner fixture tests
- use-in-place versus managed-copy tests
- no-build-time-network test

Runtime:

- Content Manager opens with no document.
- Existing distribution scan lists findings before changes.
- Managed install shows progress and can be cancelled safely.
- Verify and repair work after deliberate fixture corruption.
- Remove does not touch external files.
- Key rebinding affects Metal Visual Mode.
- Sensitivity, invert Y, speed, and FOV update.
- Settings survive application restart.
- Holding movement keys produces smooth continuous movement.
- `P`, `R`, and `I` still operate.
- Camera movement does not dirty the map document.
