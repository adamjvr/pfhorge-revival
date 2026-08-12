#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"VM-4A validation failed: {message}")


def text(root: Path, relative: str) -> str:
    path = root / relative
    require(path.is_file(), f"missing file: {relative}")
    return path.read_text(encoding="utf-8")


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()

    scene = text(root, "Pfhorge Source/Preview/Core/PreviewScene.hpp")
    collision = text(root, "Pfhorge Source/Preview/Core/PreviewCollision.hpp")
    builder = text(root, "Pfhorge Source/Preview/Core/PreviewSceneBuilder.hpp")
    texture = text(root, "Pfhorge Source/Preview/Core/PreviewTexture.hpp")
    metal = text(root, "Pfhorge Source/Preview/Metal/PfhorgeMetalPreviewView.inc")
    settings = text(root, "Pfhorge Source/Content/PfhorgeVisualModeSettings.h")
    sync = text(root, "Pfhorge Source/Content/PfhorgeLevelTextureSync.h")
    manager = text(root, "Pfhorge Source/Content/PfhorgeContentManager.inc")
    level = text(root, "Pfhorge Source/Data Objects/Map-Level Code/LELevelData.m")
    inspector = text(root, "Pfhorge Source/View and Controller/Inspector/PhTextureInspectorController.m")
    repository = text(root, "Pfhorge Source/Other Sources/TextureRepository.swift")
    makefile = text(root, "revival.mk")

    for marker in (
        "PreviewPlatform",
        "PreviewTextureAudit",
        "wallSegmentsWithoutSide",
        "platforms",
    ):
        require(marker in scene, f"PreviewScene marker missing: {marker}")

    for marker in (
        "MovePreviewCameraWithCollision",
        "PreviewResolvePosition",
        "FindPreviewDoorForInteraction",
        "wall sliding",
    ):
        require(marker in collision, f"collision marker missing: {marker}")

    for marker in (
        "resolvedSideForPolygon",
        "textureDescriptorForSideDefinition",
        "buildPlatformGeometry",
        "ComputePreviewLevelFingerprint",
        "PreviewSceneBuildOptions",
        "wallSegmentsWithTexture",
        "side.primaryTextureStruct",
        "line.clockwisePolygonSideObject",
        "line.counterclockwisePolygonSideObject",
        "candidate.lineObject == line",
        "rejectedStalePolygonSides",
        "SurfaceTextureLayer::Transparent",
        "transparentWallSegments",
    ):
        require(marker in builder, f"scene-builder marker missing: {marker}")

    require("[polygon sideIndexesAtIndex:" not in builder,
            "unsafe nil-to-side-zero polygon fallback is still present")
    require(builder.index("line.clockwisePolygonObject == polygon") <
            builder.index("if (direct != nil)"),
            "line-owned side resolution must precede the polygon cache")

    require("ResolveLevelEnvironmentCollection" in texture,
            "level environment collection resolver missing")

    for marker in (
        "VM-4A collision, doors, live level sync, and texture audit",
        "MovePreviewCameraWithCollision",
        "FindPreviewDoorForInteraction",
        "PfhorgeVMActionKeyPreference",
        "PfhorgeLevelEnvironmentDidChangeNotification",
        "platform animation",
        "Live map change applied",
        "wallTex=%u/%u",
        "sideResolve cw=%u cc=%u",
        "lookup invalid=%zu",
        "surface.translucent",
    ):
        require(marker in metal, f"Metal marker missing: {marker}")

    for marker in (
        "PfhorgeVMActionKeyPreference",
        "PfhorgeVMCollisionModePreference",
        "PfhorgeVMLiveLevelSyncPreference",
        "PfhorgeVMFollowLevelEnvironmentPreference",
        "PfhorgeRemapTexturesOnEnvironmentChangePreference",
        "@((NSInteger)0x20)",
    ):
        require(marker in settings or marker in sync,
                f"settings marker missing: {marker}")
    require("PfhorgeRemapTexturesOnEnvironmentChangePreference: @NO" in settings,
            "destructive environment remap must remain opt-in")

    for marker in (
        "Audit Active Map Textures…",
        "PfhorgeBuildActiveMapTextureAudit",
        "PfhorgeAuditSideForPolygonEdge",
        "polygonSideIndex",
        "Use / Open door",
        "Collision-aware walk mode",
        "Update open Visual Mode from unsaved map edits",
        "Remap existing classic surfaces",
        "Wall-producing edges",
    ):
        require(marker in manager, f"Content/settings marker missing: {marker}")

    require("@synthesize environmentCode" not in level,
            "legacy synthesized environmentCode accessor still present")
    require(level.count("- (void)setEnvironmentCode:(short)value") == 1,
            "custom environment setter must occur exactly once")

    for marker in (
        "PfhorgeRemapLevelClassicTextures",
        "setEnvironmentCode:(short)value",
        "PfhorgeLevelEnvironmentDidChangeNotification",
        "remappedTextureCount",
        "editorCollection >= 17",
    ):
        require(marker in level, f"level sync marker missing: {marker}")

    for marker in (
        "PfhorgeEffectiveInspectorCollection",
        "currentEnvironment = -1",
        "floorTextureChar >= 0",
        "PfhorgeVMFollowLevelEnvironmentPreference",
    ):
        require(marker in inspector, f"inspector marker missing: {marker}")

    for marker in (
        "synchronizeSelectedShapes",
        "loadNormalizedCollection",
        "textureForCollection:bitmap:",
        "classicTextureAuditSummary",
        "loadErrors",
    ):
        require(marker in repository, f"TextureRepository marker missing: {marker}")

    require(metal.count("- (void)toggleNearestDoor") == 2,
            "Metal door method must have one declaration and one implementation")
    require(metal.count("selector:@selector(levelEnvironmentDidChange:)") == 1,
            "level environment observer must occur exactly once")
    require(manager.count("Audit Active Map Textures…") >= 2,
            "texture audit must be available in settings and the Content menu")
    require("TEX-1A.1 preview schema compatibility" in
            text(root, "scripts/revival/validate_tex1a.py"),
            "inherited TEX-1A validator was not updated for scene revision 5")

    require("vm4a-check" in makefile, "revival.mk vm4a-check target missing")
    require("validate_vm4a.sh" in makefile,
            "revival.mk does not invoke validate_vm4a.sh")

    print("VM-4A / TEX-1A.1 / LEVEL-SYNC-1A portable validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
