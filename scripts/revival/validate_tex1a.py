#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import sys
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"TEX-1A validation failed: {message}")


def require(path: Path) -> str:
    if not path.is_file():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def markers(path: Path, required: tuple[str, ...]) -> None:
    text = require(path)
    for marker in required:
        if marker not in text:
            fail(f"{path}: marker missing: {marker}")


def require_any_marker(path: Path, required: tuple[str, ...]) -> None:
    text = require(path)
    if not any(marker in text for marker in required):
        fail(
            f"{path}: marker group missing: "
            + " or ".join(required)
        )


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()

    markers(
        root / "Pfhorge Source/Other Sources/TextureRepository.swift",
        (
            "@objc(textureForCollection:bitmap:)",
            "func loadAllTextures()",
            "@objc(reloadClassicTextures)",
            "case 17...21, 27...30",
            "return collection - 17",
            "images.indices.contains(index)",
            "@objc(texturesForEnvironment:)",
        ),
    )
    markers(
        root / "Pfhorge Source/Preview/Core/PreviewTexture.hpp",
        (
            "struct PreviewTextureKey final",
            "NormalizeClassicCollection",
            "IsClassicTextureCollection",
            "ClassicTextureKeyFor",
            "ClassicSurfaceOpacity",
        ),
    )
    # TEX-1A.1 preview schema compatibility: the scene revision advances
    # from 4 to 5 for platform state and texture-audit metadata, while all
    # classic Shapes requirements below remain mandatory.
    require_any_marker(
        root / "Pfhorge Source/Preview/Core/PreviewSceneBuilder.hpp",
        ("scene.revision = 4U", "scene.revision = 5U"),
    )
    markers(
        root / "Pfhorge Source/Preview/Core/PreviewSceneBuilder.hpp",
        (
            '#import "LESide.h"',
            '#import "PhMedia.h"',
            "wallTextureSelection",
            "side.secondaryTextureStruct",
            "polygon.floorOrigin",
            "polygon.ceilingOrigin",
            "SurfaceKind::Media",
            "SurfaceKind::Landscape",
            "LELineLandscape",
            "media.origin",
        ),
    )
    markers(
        root / "Pfhorge Source/Preview/Metal/PfhorgeMetalPreviewView.inc",
        (
            "TEX-1A classic Shapes texture rendering",
            "simd_float2 textureCoordinate",
            "classicTexture.sample",
            "MetalPreviewDrawRange",
            "MTKTextureLoaderOptionGenerateMipmaps",
            "MTKTextureLoaderOptionOrigin",
            "PfhorgeVMTextureFilteringPreference",
            "PfhorgeVMAnisotropyPreference",
            "PfhorgeVMUntexturedDiagnosticPreference",
            "PfhorgeContentSelectionDidChangeNotification",
            "drawPass(false)",
            "drawPass(true)",
            "textured=%zu fallback=%zu cache=%lu",
        ),
    )
    markers(
        root / "Pfhorge Source/Preview/Tests/PreviewTextureSmoke.cpp",
        (
            "NormalizeClassicCollection(17) == 0",
            "NormalizeClassicCollection(30) == 13",
            "ClassicSurfaceOpacity(SurfaceKind::Media)",
        ),
    )
    markers(
        root / "revival.mk",
        (
            "tex1a-check:",
            "scripts/revival/validate_tex1a.sh",
            "preview-core-check",
            "content1a2-check",
        ),
    )

    metal = require(
        root / "Pfhorge Source/Preview/Metal/PfhorgeMetalPreviewView.inc"
    )
    if metal.count("- (void)rebuildSamplerState\n{") != 1:
        fail("Metal sampler implementation must occur exactly once")
    if metal.count("selector:@selector(contentSelectionDidChange:)") != 1:
        fail("content-selection observer must occur exactly once")
    if metal.count("[[nodiscard]] NSString *MetalPreviewShaderSource()") != 1:
        fail("Metal shader source must occur exactly once")

    # TEX-1A must remain independent of optional enhanced-pack payloads. The
    # selected Shapes file is the canonical source for this phase.
    forbidden = (
        "Marathon-1-Best-Available-HD-Visual-Pack",
        "Marathon-2-CFP-Complete-HD",
        "Marathon-Infinity-CFP-Complete-HD",
    )
    for value in forbidden:
        if value in metal:
            fail(f"classic renderer improperly depends on enhanced pack: {value}")

    print("TEX-1A portable validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
