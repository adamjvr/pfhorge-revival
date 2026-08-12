#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import argparse
import importlib.util
import json
import tempfile
import sys
import zipfile
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"CONTENT-1A validation failed: {message}")


def load_probe(path: Path):
    spec = importlib.util.spec_from_file_location("content_registry_probe", path)
    require(spec is not None and spec.loader is not None, "could not load content probe")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    root = args.root.resolve()

    manager = root / "Pfhorge Source/Content/PfhorgeContentManager.inc"
    settings = root / "Pfhorge Source/Content/PfhorgeVisualModeSettings.h"
    metal = root / "Pfhorge Source/Preview/Metal/PfhorgeMetalPreviewView.inc"
    delegate_candidates = [
        path for path in (root / "Pfhorge Source").rglob("LEDelegate.m")
        if path.is_file() and "@implementation LEDelegate" in path.read_text(encoding="utf-8")
    ]
    require(
        len(delegate_candidates) == 1,
        f"expected one LEDelegate.m implementation, found {len(delegate_candidates)}",
    )
    delegate = delegate_candidates[0]
    visual_header = root / "Pfhorge Source/Visual Mode Code/MyOpenGL/MyOpenGLView2.h"
    probe_path = root / "scripts/revival/content_registry_probe.py"

    for path in (manager, settings, metal, visual_header, probe_path):
        require(path.is_file(), f"missing {path.relative_to(root)}")

    manager_text = manager.read_text(encoding="utf-8")
    settings_text = settings.read_text(encoding="utf-8")
    metal_text = metal.read_text(encoding="utf-8")
    delegate_text = delegate.read_text(encoding="utf-8")
    header_text = visual_header.read_text(encoding="utf-8")

    # Later Content Manager phases retained the original installation and
    # safety behavior while replacing several user-facing labels. Accept each
    # supported UI generation so this inherited validator tests capabilities
    # instead of presentation text from CONTENT-1A.
    manager_marker_groups = (
        ("Pfhorge Content Manager",),
        (
            "Install Official",
            "Install / Reinstall Official…",
        ),
        (
            "Import Texture Pack",
            "Install Recommended Enhanced",
            "Install / Rebuild Recommended…",
        ),
        ("api.github.com/repos/Aleph-One-Marathon/alephone/releases/latest",),
        ("PfhorgeValidateZipArchive",),
        ("PfhorgeAuditManagedTree",),
        ("Use in Place",),
        (
            "Repair / Reinstall",
            "Re-select Source to Repair",
        ),
        (
            "Open Manifest",
            "openManifest:",
        ),
        ("Copy into Pfhorge",),
        ("Visual Mode & GPU Settings",),
        ("Anisotropic filtering",),
    )
    for alternatives in manager_marker_groups:
        require(
            any(marker in manager_text for marker in alternatives),
            "Content Manager marker group missing: "
            + " or ".join(alternatives),
        )

    required_settings_markers = (
        "PfhorgeVMForwardKeyPreference",
        "PfhorgeVMMouseSensitivityPreference",
        "PfhorgeVMPreferredMetalRegistryIDPreference",
        "PfhorgeVMTextureFilteringPreference",
        "PfhorgeVMAnisotropyPreference",
        "PfhorgeVMDiagnosticsOverlayPreference",
    )
    for marker in required_settings_markers:
        require(marker in settings_text, f"settings marker missing: {marker}")

    required_metal_markers = (
        "CONTENT-1A continuous input",
        "updateCameraForFrame",
        "PfhorgeVMForwardKeyPreference",
        "PfhorgeVisualModeSettingsDidChangeNotification",
        "updateDiagnosticsOverlay",
        "windowDidResignKey",
        "drawableSizeWillChange",
        "PfhorgePreferredMetalDevice",
    )
    for marker in required_metal_markers:
        require(marker in metal_text, f"Metal integration marker missing: {marker}")

    require("PfhorgeContentManager.inc" in delegate_text, "delegate does not include Content Manager")
    require("PfhorgeInstallContentAndVisualModeMenus" in delegate_text, "menus are not installed")
    require("PfhorgeVisualModeSettings.h" in header_text, "Visual Mode header does not import settings")

    probe = load_probe(probe_path)
    with tempfile.TemporaryDirectory(prefix="pfhorge-content1a-") as temp:
        temp_path = Path(temp)
        distribution = temp_path / "Classic Marathon 2"
        (distribution / "Plugins" / "HD Walls").mkdir(parents=True)
        (distribution / "Shapes.shpA").write_bytes(b"S" * 4096)
        (distribution / "Plugins" / "HD Walls" / "Walls.mml").write_text("<marathon/>", encoding="utf-8")
        (distribution / "Plugins" / "HD Walls" / "wall01.png").write_bytes(b"P" * 2048)
        (distribution / "LICENSE.txt").write_text("fixture license", encoding="utf-8")

        report = probe.scan(distribution)
        require(len(report.shapes) == 1, "directory scanner did not find Shapes")
        require(len(report.mml) == 1, "directory scanner did not find MML")
        require(len(report.textures) == 1, "directory scanner did not find texture")
        require(len(report.plugins) >= 1, "directory scanner did not find plugin directory")
        require(len(report.rights_documents) == 1, "directory scanner did not find rights document")

        builder = temp_path / "Builder"
        builder.mkdir()
        (builder / "build_pack.py").write_text("CONFIG = {}", encoding="utf-8")
        builder_report = probe.scan(builder)
        require(len(builder_report.builder_recipes) == 1, "builder recipe was not detected")

        safe_zip = temp_path / "safe.zip"
        with zipfile.ZipFile(safe_zip, "w") as archive:
            archive.writestr("Marathon 2/Shapes.shpA", b"S" * 4096)
            archive.writestr("Marathon 2/Plugins/HD/Walls.mml", "<marathon/>")
            archive.writestr("Marathon 2/Plugins/HD/wall.png", b"P" * 2048)
        zip_report = probe.scan(safe_zip)
        require(not zip_report.unsafe_archive_entries, "safe ZIP marked unsafe")
        require(zip_report.shapes and zip_report.mml and zip_report.textures, "ZIP scanner missed content")

        unsafe_zip = temp_path / "unsafe.zip"
        with zipfile.ZipFile(unsafe_zip, "w") as archive:
            archive.writestr("../escape.txt", "bad")
            archive.writestr("Shapes.shpA", b"S" * 4096)
        unsafe_report = probe.scan(unsafe_zip)
        require("../escape.txt" in unsafe_report.unsafe_archive_entries, "path traversal was not rejected")

        collision_zip = temp_path / "collision.zip"
        with zipfile.ZipFile(collision_zip, "w") as archive:
            archive.writestr("Textures/Wall.png", b"P" * 2048)
            archive.writestr("textures/wall.png", b"P" * 2048)
        collision_report = probe.scan(collision_zip)
        require(
            "textures/wall.png" in collision_report.unsafe_archive_entries,
            "case-colliding archive entries were not rejected",
        )

        original_limit = probe.MAX_TOTAL_BYTES
        try:
            probe.MAX_TOTAL_BYTES = 1024
            oversize_zip = temp_path / "oversize.zip"
            with zipfile.ZipFile(oversize_zip, "w") as archive:
                archive.writestr("Textures/large.png", b"P" * 2048)
            oversize_report = probe.scan(oversize_zip)
            require(oversize_report.truncated, "expanded-size limit was not enforced")
        finally:
            probe.MAX_TOTAL_BYTES = original_limit

    print("CONTENT-1A / VM-SETTINGS-1A portable validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
