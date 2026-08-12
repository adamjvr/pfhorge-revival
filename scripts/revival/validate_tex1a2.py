#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"TEX-1A.2 validation failed: {message}")


def text(root: Path, relative: str) -> str:
    path = root / relative
    require(path.is_file(), f"missing file: {relative}")
    return path.read_text(encoding="utf-8")


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()

    types = text(root, "Pfhorge Source/Preview/Core/PreviewTypes.hpp")
    scene = text(root, "Pfhorge Source/Preview/Core/PreviewScene.hpp")
    builder = text(root, "Pfhorge Source/Preview/Core/PreviewSceneBuilder.hpp")
    texture = text(root, "Pfhorge Source/Preview/Core/PreviewTexture.hpp")
    metal = text(root, "Pfhorge Source/Preview/Metal/PfhorgeMetalPreviewView.inc")
    polygon = text(
        root,
        "Pfhorge Source/Data Objects/Marathon Data/LEPolygon.m",
    )
    line = text(
        root,
        "Pfhorge Source/Data Objects/Marathon Data/LELine.m",
    )
    roadmap = text(root, "docs/revival/VISUAL-MODE-ROADMAP.md")
    vm4a = text(root, "scripts/revival/validate_vm4a.py")
    makefile = text(root, "revival.mk")

    for marker in (
        "SurfaceTextureLayer",
        "Primary",
        "Secondary",
        "Transparent",
    ):
        require(marker in types, f"surface texture-layer marker missing: {marker}")

    for marker in (
        "lineID",
        "edgeIndex",
        "textureLayer",
        "translucent",
        "sidesResolvedClockwise",
        "sidesResolvedCounterclockwise",
        "sidesResolvedDirect",
        "rejectedStalePolygonSides",
        "sideResolutionMisses",
        "transparentWallSegments",
    ):
        require(marker in scene, f"PreviewScene audit marker missing: {marker}")

    require(
        "(side_objects[i] == nil) ? 0" in polygon,
        "test premise changed: polygon nil-side accessor no longer aliases zero",
    )
    require(
        "(clockwisePolygonSideObject == nil) ? -1" in line,
        "clockwise line-side accessor no longer preserves NONE semantics",
    )
    require(
        "(counterclockwisePolygonSideObject == nil) ? -1" in line,
        "counterclockwise line-side accessor no longer preserves NONE semantics",
    )

    for marker in (
        "resolvedSideForPolygon",
        "line.clockwisePolygonObject == polygon",
        "line.conterclockwisePolygonObject == polygon",
        "if (direct != nil)",
        "rejectedStalePolygonSides",
        "sideTextureDefinitionHasReference",
        "SurfaceTextureLayer::Transparent",
        "side.transparentTextureStruct",
        "side.transparentTransferMode",
        "transparentWallSegments",
        "wall.translucent = translucent",
    ):
        require(marker in builder, f"builder marker missing: {marker}")

    require(
        "[polygon sideIndexesAtIndex:" not in builder,
        "unsafe polygon-side index fallback remains active",
    )
    require(
        builder.index("line.clockwisePolygonObject == polygon")
        < builder.index("if (direct != nil)"),
        "line-owned side resolution must precede polygon-cache recovery",
    )
    require(
        builder.index("SurfaceTextureLayer::Transparent")
        < builder.index("detail::appendWallSegment(", builder.index(
            "sideTextureDefinitionHasReference(")),
        "transparent texture selection is not wired before draw creation",
    )

    require(
        "ClassicSurfaceOpacity(\n    SurfaceKind kind)" in texture,
        "inherited SurfaceKind opacity compatibility overload missing",
    )
    require(
        "ClassicSurfaceOpacity(\n    const PreviewSurface& surface)" in texture,
        "surface-aware classic opacity helper missing",
    )
    require(
        "kind == SurfaceKind::Media" in texture,
        "media opacity behavior was lost",
    )
    require(
        "return ClassicSurfaceOpacity(surface.id.kind);" in texture,
        "surface-aware opacity helper does not delegate compatibly",
    )

    for marker in (
        "ClassicTextureFailure",
        "NegativeCache",
        "RepositoryUnavailable",
        "ImageConversionFailed",
        "UploadFailed",
        "surface.translucent",
        "sideResolve cw=%u cc=%u",
        "lookup invalid=%zu",
        "transparent=%u",
        "clearClassicTextureCache",
    ):
        require(marker in metal, f"Metal audit marker missing: {marker}")

    require(
        metal.count("[self clearClassicTextureCache];") >= 3,
        "content, settings, and environment cache invalidation are incomplete",
    )

    for marker in (
        "Current stabilization gate — no commit yet",
        "TEX-1A.2 — surface completeness and wall resolution",
        "Companion Visual Mode Texture Palette",
        "separate AppKit `NSWindow`",
        "preserve the existing 2D Texture Inspector unchanged and canonical",
        "never introduce Visual Mode-only texture state",
    ):
        require(marker in roadmap, f"roadmap marker missing: {marker}")

    require(
        '"sideIndexesAtIndex",' not in vm4a,
        "inherited VM-4A validator still requires the unsafe fallback",
    )
    require(
        "unsafe nil-to-side-zero polygon fallback is still present" in vm4a,
        "inherited VM-4A validator does not reject the unsafe fallback",
    )
    require("tex1a2-check" in makefile, "revival.mk target missing")
    require(
        "validate_tex1a2.sh" in makefile,
        "revival.mk does not invoke TEX-1A.2 validation",
    )

    print("TEX-1A.2 wall completeness portable validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
