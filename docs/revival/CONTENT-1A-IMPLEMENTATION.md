# CONTENT-1A / VM-SETTINGS-1A — Implementation Foundation

## Delivered in this package

### Unified Content Manager

Pfhorge adds **Pfhorge → Content Manager…** and manages the following from one
window:

- Classic Marathon
- Marathon 2: Durandal
- Marathon Infinity
- custom scenarios
- original Shapes sources and their embedded original textures
- external textures and MML bundled with a distribution
- imported Aleph One plugins and texture-pack ZIPs

Supported source workflows:

- install the current official trilogy data archive from the Aleph One GitHub
  release
- scan common local installation locations
- choose an existing distribution, scenario folder, or Shapes file
- use the selected content in place
- copy it into Pfhorge-managed Application Support storage
- verify, repair/reinstall, reveal, open its provenance manifest, remove, or unregister a source
- import an external texture pack from a directory, ZIP, MML, or image
- recognize the supplied builder archives without executing imported code

Official downloads are discovered from the latest release metadata at runtime.
The selected asset must include a GitHub-published SHA-256 digest. Pfhorge then:

1. downloads after explicit confirmation,
2. verifies SHA-256,
3. rejects absolute paths, traversal entries, case collisions, symbolic/hard links, excessive file counts, and oversized expansion,
4. extracts into an isolated staging directory,
5. scans for Shapes/MML/external textures,
6. atomically promotes the validated content,
7. writes a provenance manifest containing Shapes hashes, discovered rights documents, source information, and the active profile.

No network access occurs during compilation.

Managed content lives under:

```text
~/Library/Application Support/Pfhorge/Content/
├── Downloads/
├── Distributions/
├── Staging/
├── Texture Profiles/
└── Manifests/
```

Removing a managed copy never removes an independent Marathon installation.
Unregistering an external source never modifies it.

### Content profiles

Each game manifest records one active profile:

- Original
- Distribution Default
- Enhanced
- Custom
- Untextured Diagnostic

The intended resolver order for TEX-1A/TEX-1B is:

```text
project override
→ selected imported/HD profile
→ enabled distribution plugin
→ original texture embedded in Shapes
→ diagnostic checkerboard
```

This package catalogs the sources. It does not yet sample those textures in the
Metal shader; that is TEX-1A.

### Visual Mode and GPU settings

Pfhorge adds **View → Visual Mode & GPU Settings…** with four tabs:

- Controls
- Display & GPU
- Textures & Content
- Diagnostics

The Metal implementation now consumes persistent settings for:

- rebindable forward/back/strafe/fly controls
- reset, orbit diagnostic, and diagnostics keys
- mouse sensitivity and invert Y
- movement speed
- field of view
- frame-rate limit
- VSync
- render scale
- MSAA selected for the next Visual Mode session
- preferred Metal device selected for the next Visual Mode session
- persisted texture filtering and anisotropy for TEX-1A samplers
- optional on-screen portal/camera diagnostics

Movement uses pressed-key state and frame delta instead of one discrete movement
step for every macOS key-repeat event. Pressed keys are cleared when a window loses
focus, and portal visibility is rebuilt when drawable size or FOV changes.

Default controls:

| Action | Key |
|---|---|
| Forward | W |
| Backward | S |
| Strafe left | A |
| Strafe right | D |
| Fly down | Q |
| Fly up | E |
| Reset camera | R |
| Orbit diagnostic | P |
| Diagnostics | I |

The scroll wheel remains a per-session speed multiplier in first-person mode and
controls orbit distance in diagnostic mode.

## Validation

Portable validation:

```bash
make -f revival.mk content1a-check
```

macOS build:

```bash
make -f revival.mk baseline
```

runtime:

1. Open **Pfhorge → Content Manager…** with no document open.
2. Register a local Marathon 2 distribution using **Choose Existing…**.
3. Confirm both **Use in Place** and **Copy into Pfhorge** behave correctly.
4. Verify and reveal the registered source.
5. Import an Aleph One plugin or generated HD-pack ZIP. Builder recipe ZIPs are registered safely and instruct the user to run the builder externally; Pfhorge never executes imported scripts automatically.
6. Open **View → Visual Mode & GPU Settings…**.
7. Rebind movement and diagnostic keys and apply.
8. Open an imported level in Metal Visual Mode.
9. Hold movement keys and confirm smooth continuous movement.
10. Confirm sensitivity, invert Y, FOV, FPS, VSync, render scale, and overlay.
11. Close and reopen Pfhorge and confirm persistence.
12. Confirm movement still does not dirty the map document.

## Deferred to TEX-1A

- conversion of decoded Shapes images to `MTLTexture`
- textured floors and ceilings
- primary, secondary, and transparent wall materials
- texture offsets and transfer modes
- lighting, liquids, landscapes, and animated textures
- HD replacement sampling in the Metal shader
