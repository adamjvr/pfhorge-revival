# CONTENT-1A Runtime Test Checklist

## Content Manager

- [ ] Opens from **Pfhorge → Content Manager…** with no document.
- [ ] Four source categories appear.
- [ ] **Choose Existing…** finds Shapes, MML, and external textures.
- [ ] **Use in Place** registers without copying or modifying the source.
- [ ] **Copy into Pfhorge** creates a managed copy.
- [ ] **Scan This Mac** presents discovered Shapes-containing folders.
- [ ] **Install Official…** shows source, size, and SHA-256 before download.
- [ ] Download progress updates and cancellation works.
- [ ] A digest mismatch or unsafe ZIP is rejected.
- [ ] **Verify** rescans the registered source.
- [ ] **Repair / Reinstall…** reinstalls an official source or asks for the original external source.
- [ ] **Open Manifest…** opens provenance, Shapes hashes, and rights-document inventory.
- [ ] **Reveal** opens Finder at the active source.
- [ ] Removing a managed copy does not touch any external installation.
- [ ] Unregistering an external source does not modify it.
- [ ] **Import Texture Pack…** catalogs MML and external images.
- [ ] A supplied builder ZIP is recognized as a recipe and is not executed automatically.
- [ ] Original, Distribution Default, Enhanced, Custom, and Untextured profiles persist.

## Visual Mode settings

- [ ] Opens from **View → Visual Mode & GPU Settings…**.
- [ ] Key bindings reject duplicates.
- [ ] Settings persist across application restart.
- [ ] Holding movement keys produces continuous movement.
- [ ] Switching windows while holding a key does not leave movement stuck.
- [ ] Mouse sensitivity and invert Y apply live.
- [ ] FOV and frame-rate changes apply live.
- [ ] Render scale changes the drawable resolution.
- [ ] GPU and MSAA selection apply on the next Visual Mode entry.
- [ ] Texture filtering and anisotropy selections persist for TEX-1A.
- [ ] Diagnostics overlay can be toggled.
- [ ] Rebound P, R, and I actions work.
- [ ] Camera movement does not mark a clean level document as edited.
