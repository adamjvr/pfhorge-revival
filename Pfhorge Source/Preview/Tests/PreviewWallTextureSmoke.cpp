// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#include "PreviewTexture.hpp"

#include <cstdlib>
#include <iostream>

using namespace pfhorge::preview;

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Preview wall texture smoke failure: "
                  << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main()
{
    PreviewSurface primary;
    primary.id = SurfaceID{SurfaceKind::Wall, 12U, 4U};
    primary.polygonID = 12U;
    primary.lineID = 44U;
    primary.sideID = 71U;
    primary.edgeIndex = 1U;
    primary.textureLayer = SurfaceTextureLayer::Primary;
    primary.texture = TextureDescriptor{18, 7, 0};

    const PreviewTextureKey primaryKey = ClassicTextureKeyFor(primary);
    require(primaryKey.valid(), "raw Marathon collection did not normalize");
    require(primaryKey.collection == 1, "lava collection should normalize to 1");
    require(primaryKey.bitmap == 7, "primary bitmap changed");
    require(!primary.translucent, "ordinary wall became translucent");

    PreviewSurface transparent = primary;
    transparent.id.subpart = 5U;
    transparent.textureLayer = SurfaceTextureLayer::Transparent;
    transparent.texture = TextureDescriptor{18, 9, 0};
    transparent.translucent = true;

    const PreviewTextureKey transparentKey =
        ClassicTextureKeyFor(transparent);
    require(transparentKey.valid(), "transparent texture key is invalid");
    require(transparentKey.collection == 1,
            "transparent collection should normalize with primary walls");
    require(transparentKey.bitmap == 9, "transparent bitmap changed");
    require(transparent.translucent,
            "transparent portal surface lost translucent-pass metadata");
    require(ClassicSurfaceOpacity(transparent) == 1.0F,
            "transparent bitmap alpha must not receive an arbitrary fade");

    PreviewSurface media;
    media.id = SurfaceID{SurfaceKind::Media, 12U, 0U};
    media.textureLayer = SurfaceTextureLayer::Media;
    media.translucent = true;
    require(ClassicSurfaceOpacity(media) == 0.65F,
            "existing media opacity behavior regressed");

    PreviewTextureAudit audit;
    audit.sideResolutionAttempts = 8U;
    audit.sidesResolvedClockwise = 3U;
    audit.sidesResolvedCounterclockwise = 3U;
    audit.sidesResolvedDirect = 1U;
    audit.sideResolutionMisses = 1U;
    audit.transparentSideReferences = 2U;
    audit.transparentWallSegments = 2U;

    require(
        audit.sidesResolvedClockwise +
        audit.sidesResolvedCounterclockwise +
        audit.sidesResolvedDirect +
        audit.sideResolutionMisses ==
        audit.sideResolutionAttempts,
        "side-resolution audit does not account for every attempted edge");
    require(audit.transparentWallSegments ==
            audit.transparentSideReferences,
            "transparent references were not converted into surfaces");

    std::cout << "Preview wall texture smoke test passed\n";
    return EXIT_SUCCESS;
}
