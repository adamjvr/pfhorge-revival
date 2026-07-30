# CONTENT-1A.2 / VM-SETTINGS-1B.1

## Scope

This phase completes the content-selection UX and high-refresh display controls before TEX-1A begins Metal texture sampling.

### Content Manager

- Separates required Base Game Data / Shapes from optional Enhanced Texture Appearance.
- Displays selectable full paths for the active Shapes file and enhanced profile.
- Provides contextual Copy Path, Reveal, Verify, Find Existing, Reinstall, Import, and Remove controls.
- Removes the generic Advanced Actions popup.
- Automatically chains official original-data installation before an enhanced build when Shapes is missing.
- Never deletes an external content source. Only managed enhanced profiles inside Pfhorge's Texture Profiles directory are eligible for deletion.
- Adds a dedicated Content top-level menu with manager, reload, active-game, active-appearance, and Reveal commands.

### Progress

- Official downloads use byte progress when a total is known and an animated indeterminate state otherwise.
- Reviewed builders emit `PFHORGE_PROGRESS` JSON records for resolve, download, verification, extraction, assembly, packaging, verification, and completion.
- Pfhorge parses those records while teeing all output into `builder.log`.
- Cancel terminates the builder and its immediate child downloader processes.

### Display

- Adds Display Maximum plus 30/60/90/120/144/165/240 Hz presets.
- Display Maximum uses the current screen's `maximumFramesPerSecond` and updates when the window changes screens.
- The diagnostics overlay remains the runtime source of truth for achieved FPS.

## Scope boundary

This phase installs, selects, verifies, and activates source content. Original Shapes image decoding and Metal surface sampling remain TEX-1A. Enhanced replacement sampling remains TEX-1B.
