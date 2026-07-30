#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import argparse
import ast
import json
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

    for marker in (
        "Original game appearance",
        "Enhanced appearance (optional)",
        "Install Recommended Enhanced",
        "PfhorgeActivateContentProfile",
        "PfhorgeLegacyShapesPathPreference",
        "sharedTextureRepository",
        "loadAllTextures",
        "updateTextureMenuContents",
        "Marathon2-%@-Data.zip",
        "MarathonInfinity-%@-Data.zip",
        "beginRecommendedEnhancedBuildForGame",
        "selectedEnhancedProfileID",
        "Enhanced artwork always falls back",
    ):
        require(marker in manager_text, f"Content Manager marker missing: {marker}")

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
        "migrationVersion >= 2",
    ):
        require(marker in settings_text, f"settings marker missing: {marker}")

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
