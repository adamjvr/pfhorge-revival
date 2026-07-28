# Forge parity matrix

Status values:

- `PASS`
- `PARTIAL`
- `BROKEN`
- `NOT TESTED`
- `NOT IMPLEMENTED`
- `ALEPH-ONE-ONLY`
- `OUT OF SCOPE — ANVIL`

| Capability | Status | Evidence / next test |
|---|---|---|
| Draw and edit lines | NOT TESTED | Build minimal geometry fixture |
| Fill polygons | NOT TESTED | Verify legal and malformed loops |
| Polygon heights | NOT TESTED | Save/reopen and Aleph One load |
| Object placement | NOT TESTED | Test type, facing, height, flags |
| Platforms | NOT TESTED | Flags, tags, min/max heights |
| Switch/tag relationships | NOT TESTED | Visual and runtime validation |
| Lights | NOT TESTED | Definition and assignment round trip |
| Liquids/media | NOT TESTED | Heights, textures, transfer modes |
| Ambient/random sounds | NOT TESTED | Definition and assignment |
| Terminal editing | NOT TESTED | Branches, teleports, pictures |
| Scenario compilation | NOT TESTED | Multi-level fixture |
| Scenario extraction | NOT TESTED | Preserve terminal and metadata |
| 3D navigation | BROKEN | Legacy OpenGL preview is known broken |
| Wall texturing in 3D | NOT TESTED | VM-6 |
| Floor/ceiling texturing in 3D | NOT TESTED | VM-6 |
| Texture alignment/offsets | NOT TESTED | VM-6 |
| Transfer modes | NOT TESTED | VM-4/VM-6 |
| Floor/ceiling editing in 3D | NOT TESTED | VM-6 |
| Visual object manipulation | NOT TESTED | VM-6 |
| Marathon 1 maps | NOT TESTED | Licensed fixture required |
| Marathon 2 maps | NOT TESTED | Licensed fixture required |
| Marathon Infinity maps | NOT TESTED | Terminal fixture required |
| Current Aleph One extensions | NOT IMPLEMENTED | Separate compatibility layer |
| Physics model editing | OUT OF SCOPE — ANVIL | Integrate external tools later |
| Shapes/sprite editing | OUT OF SCOPE — ANVIL | ShapeFusion/companion workflow |
| Sound-bank editing | OUT OF SCOPE — ANVIL | External asset workflow |
