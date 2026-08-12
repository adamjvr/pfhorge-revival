#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import argparse
import ast
import json
import re
import subprocess
import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"CONTENT-1A.1 validation failed: {message}")


def balanced(text: str, opening: str, closing: str) -> bool:
    return text.count(opening) == text.count(closing)


def extract_config(builder: Path) -> dict:
    tree = ast.parse(builder.read_text(encoding="utf-8"), filename=str(builder))
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id == "CONFIG":
                    value = ast.literal_eval(node.value)
                    require(isinstance(value, dict), f"CONFIG is not a dictionary in {builder}")
                    return value
    raise SystemExit(f"CONTENT-1A.1 validation failed: CONFIG missing in {builder}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    root = args.root.resolve()

    manager = root / "Pfhorge Source/Content/PfhorgeContentManager.inc"
    settings = root / "Pfhorge Source/Content/PfhorgeVisualModeSettings.h"
    metal = root / "Pfhorge Source/Preview/Metal/PfhorgeMetalPreviewView.inc"
    builders = root / "scripts/revival/content-builders"
    for path in (manager, settings, metal):
        require(path.is_file(), f"missing {path.relative_to(root)}")

    manager_text = manager.read_text(encoding="utf-8")
    settings_text = settings.read_text(encoding="utf-8")
    metal_text = metal.read_text(encoding="utf-8")

    # CONTENT-1A.2 deliberately replaced the original CONTENT-1A.1 card
    # labels while preserving the same underlying installation, activation,
    # Shapes fallback, and reviewed-builder behavior. Accept either UI
    # generation here so inherited validation checks behavior instead of stale
    # presentation text.
    manager_marker_groups = (
        (
            "Original game appearance",
            "Base game data / Shapes — Required",
        ),
        (
            "Enhanced appearance (optional)",
            "Enhanced texture appearance — Optional",
        ),
        (
            "Install Recommended Enhanced",
            "Install / Rebuild Recommended…",
        ),
        ("PfhorgeActivateContentProfile",),
        ("PfhorgeLegacyShapesPathPreference",),
        ("sharedTextureRepository",),
        ("loadAllTextures",),
        ("updateTextureMenuContents",),
        ("Marathon2-%@-Data.zip",),
        ("MarathonInfinity-%@-Data.zip",),
        ("beginRecommendedEnhancedBuildForGame",),
        ("selectedEnhancedProfileID",),
        (
            "Enhanced artwork always falls back",
            "falls back to Shapes",
        ),
    )
    for alternatives in manager_marker_groups:
        require(
            any(marker in manager_text for marker in alternatives),
            "Content Manager marker group missing: "
            + " or ".join(alternatives),
        )

    require(
        "NSMutableData *bufferStorage = [NSMutableData dataWithLength:64U * 1024U];" in manager_text,
        "heap-based SHA-256 buffer hotfix was lost",
    )
    require("uint8_t buffer[1024 * 1024]" not in manager_text, "vulnerable stack SHA buffer returned")

    for marker in (
        "PfhorgeVMMouseSensitivityXPreference",
        "PfhorgeVMMouseSensitivityYPreference",
        "PfhorgeVMInvertMouseXPreference",
        "PfhorgeVMInvertMouseYPreference",
        "PfhorgeVMLookSmoothingPreference",
        "PfhorgeVMVerticalMovementScalePreference",
        "PfhorgeVMNearPlanePreference",
    ):
        require(marker in settings_text, f"settings marker missing: {marker}")

    # Settings migrations may use either:
    #   * an early-return guard, such as migrationVersion >= currentVersion;
    #   * an upgrade-path guard, such as migrationVersion < currentVersion;
    #   * a direct persistent-preference expression; or
    #   * a named current-version constant.
    #
    # Validate the durable behavior rather than one exact source spelling.
    migration_preference = "PfhorgeVMSettingsMigrationVersionPreference"
    require(
        migration_preference in settings_text,
        "settings migration preference missing",
    )

    migration_read_present = bool(re.search(
        r"(?:migrationVersion|"
        r"persistent\s*\[\s*PfhorgeVMSettingsMigrationVersionPreference\s*\])",
        settings_text,
    ))
    require(
        migration_read_present,
        "settings migration version is never read",
    )

    migration_control_present = bool(re.search(
        r"\b(?:if|while)\s*\([\s\S]{0,700}?"
        r"(?:migrationVersion|PfhorgeVMSettingsMigrationVersionPreference)"
        r"[\s\S]{0,700}?\)"
        r"|\bswitch\s*\(\s*migrationVersion\s*\)",
        settings_text,
    ))
    require(
        migration_control_present,
        "settings migration has no version-dependent control flow",
    )

    migration_write_matches = re.findall(
        r"setInteger:\s*([A-Za-z_]\w*|\d+)\s+"
        r"forKey:\s*PfhorgeVMSettingsMigrationVersionPreference",
        settings_text,
    )
    require(
        bool(migration_write_matches),
        "settings migration target is never persisted",
    )

    migration_version_candidates = []

    # Direct writes: [defaults setInteger:3 forKey:...]
    for expression in migration_write_matches:
        if expression.isdigit():
            migration_version_candidates.append(int(expression))
            continue

        # Symbolic writes:
        #   static const NSInteger currentMigrationVersion = 3;
        #   [defaults setInteger:currentMigrationVersion forKey:...]
        symbolic_match = re.search(
            rf"\b{re.escape(expression)}\b\s*=\s*(\d+)",
            settings_text,
        )
        if symbolic_match is not None:
            migration_version_candidates.append(
                int(symbolic_match.group(1))
            )

    # Also accept explicit schema-version constants/macros even when the write
    # is wrapped in a helper or split across generated Objective-C.
    for pattern in (
        r"\b[A-Za-z_]\w*(?:Migration|Settings)\w*Version\w*"
        r"\s*=\s*(\d+)",
        r"#define\s+[A-Za-z_]\w*(?:Migration|Settings)\w*Version\w*"
        r"\s+(\d+)",
    ):
        migration_version_candidates.extend(
            int(version)
            for version in re.findall(pattern, settings_text)
        )

    require(
        any(version >= 2 for version in migration_version_candidates),
        "settings migration target is older than version 2 or could not be "
        "resolved; writes="
        + repr(migration_write_matches)
        + ", candidates="
        + repr(migration_version_candidates),
    )

    for marker in (
        "CONTENT-1A.1 independent mouse axes",
        "_mouseSensitivityX",
        "_mouseSensitivityY",
        "_invertMouseX",
        "_lookSmoothing",
        "_verticalMovementScale",
        "camera.nearPlane = _nearPlane",
        "_cameraYaw +=",
        "Window controllers and sheets can replace the first responder",
    ):
        require(marker in metal_text, f"Metal marker missing: {marker}")
    require("_cameraYaw -= deltaX * _mouseSensitivity" not in metal_text, "old inverted horizontal look remains")

    for text, name in ((manager_text, "manager"), (settings_text, "settings"), (metal_text, "Metal")):
        require(balanced(text, "{", "}"), f"unbalanced braces in {name}")
        require(balanced(text, "(", ")"), f"unbalanced parentheses in {name}")
        require(balanced(text, "[", "]"), f"unbalanced brackets in {name}")

    expected = {
        "marathon": "Marathon-1-Best-Available-HD-Visual-Pack.zip",
        "marathon2": "Marathon-2-CFP-Complete-HD.zip",
        "infinity": "Marathon-Infinity-CFP-Complete-HD.zip",
    }
    for game, output_name in expected.items():
        builder = builders / game / "build_pack.py"
        require(builder.is_file(), f"missing reviewed builder for {game}")
        config = extract_config(builder)
        require(config.get("output_zip") == output_name, f"unexpected output name for {game}")
        require(config.get("components"), f"builder has no components for {game}")
        subprocess.run(
            [sys.executable, str(builder), "--help"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

    print("CONTENT-1A.1 / VM-SETTINGS-1B portable validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
