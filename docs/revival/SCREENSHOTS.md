# Pfhorge Revival — Development Screenshots

These screenshots capture the current `main` development state on macOS in
August 2026.

They are not static mockups. They show the native application, current 2D
editor, the revived Metal Visual Mode, the Forge-style texture workspace, and
the modernized settings UI running together.

> **Development artwork note:** the splash-screen background shown below is
> part of the current development build. Rights/redistribution review for that
> background artwork must be completed before it is treated as a final
> distributable project asset.

## Startup splash

![Pfhorge startup splash](screenshots/2026-08-12/01-splash-launch.jpg)

The revived startup presents project identity and contributor/provenance
information before handing off to the editor.

## Start Center

![Pfhorge Start Center](screenshots/2026-08-12/02-start-center.jpg)

The Start Center provides native create/open/recent workflows plus direct entry
to Content, Visual Mode, and general settings.

## Full editor + Metal Visual Mode

![Pfhorge editor and Metal Visual Mode](screenshots/2026-08-12/03-editor-and-visual-mode.jpg)

This is the current working development setup: the original-style 2D map editor
alongside the revived Metal renderer and its attached texture workspace.

## Splash over a live editor session

![Splash over editor](screenshots/2026-08-12/04-splash-over-editor.jpg)

The splash/about presentation is reusable after launch and does not replace the
underlying document/editor session.

## General settings

![General settings](screenshots/2026-08-12/05-settings-general.jpg)

General editor behavior, grid display, snapping, grid resolution, and point
snap distances are grouped into a single modern native settings page.

## Colors & Themes

![Colors and Themes settings](screenshots/2026-08-12/06-settings-colors-themes.jpg)

Complete editor palettes can be selected, customized, saved, reverted, and
applied to polygon, canvas, grid, and object visualization.

## Object visibility

![Object visibility settings](screenshots/2026-08-12/07-settings-objects.jpg)

Object visibility is explicit and global for the 2D editor, while object colors
remain part of complete reusable editor themes.

## Visual Mode key bindings

![Visual Mode key bindings](screenshots/2026-08-12/08-settings-visual-mode-key-bindings.jpg)

Metal Visual Mode consumes persistent rebindable controls rather than relying on
hard-coded movement keys.

## Visual Mode display & GPU

![Visual Mode display and GPU settings](screenshots/2026-08-12/09-settings-visual-mode-display-gpu.jpg)

Current renderer controls include Metal device selection, frame-rate limit,
render scale, MSAA, texture filtering, anisotropic filtering, and VSync.

## Forge-style texture workspace

![Forge-style texture workspace](screenshots/2026-08-12/10-visual-mode-forge-texture-palette.jpg)

The Metal viewport and native AppKit texture palette now share a single Visual
Mode window. The palette displays real classic Shapes textures and publishes
selection state for the upcoming surface-picking/editing work.

## 2D editor — Death by accident

![Death by accident in the 2D editor](screenshots/2026-08-12/11-2d-editor-death-by-accident.jpg)

`Death by accident` is the current Visual Mode regression level. It exposed the
portal-adjacency bug that produced invisible collision walls and is now being
used to investigate remaining wall-surface and landscape rendering fidelity.
