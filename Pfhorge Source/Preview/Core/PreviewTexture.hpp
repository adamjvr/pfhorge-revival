// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#include "PreviewScene.hpp"

#include <cstdint>

namespace pfhorge::preview {

/// Canonical texture key used by renderer backends. Pfhorge historically
/// stores wall collections as 0...4 / 10...13 while a Marathon shape
/// descriptor stores the same collections as 17...21 / 27...30.
struct PreviewTextureKey final {
    std::int16_t collection = -1;
    std::int16_t bitmap = -1;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return collection >= 0 && bitmap >= 0;
    }

    [[nodiscard]] constexpr std::uint32_t packed() const noexcept {
        return (static_cast<std::uint32_t>(
                    static_cast<std::uint16_t>(collection))
                << 16U) |
               static_cast<std::uint16_t>(bitmap);
    }
};

[[nodiscard]] constexpr std::int16_t NormalizeClassicCollection(
    std::int16_t collection) noexcept
{
    if ((collection >= 17 && collection <= 21) ||
        (collection >= 27 && collection <= 30)) {
        return static_cast<std::int16_t>(collection - 17);
    }
    return collection;
}

[[nodiscard]] constexpr bool IsClassicWallCollection(
    std::int16_t collection) noexcept
{
    const std::int16_t normalized =
        NormalizeClassicCollection(collection);
    return normalized >= 0 && normalized <= 4;
}

[[nodiscard]] constexpr bool IsClassicLandscapeCollection(
    std::int16_t collection) noexcept
{
    const std::int16_t normalized =
        NormalizeClassicCollection(collection);
    return normalized >= 10 && normalized <= 13;
}

[[nodiscard]] constexpr bool IsClassicTextureCollection(
    std::int16_t collection) noexcept
{
    return IsClassicWallCollection(collection) ||
           IsClassicLandscapeCollection(collection);
}

[[nodiscard]] constexpr std::int16_t ResolveLevelEnvironmentCollection(
    std::int16_t collection,
    std::int16_t environmentCode,
    bool followLevelEnvironment) noexcept
{
    const std::int16_t normalized =
        NormalizeClassicCollection(collection);
    if (followLevelEnvironment &&
        normalized >= 0 && normalized <= 4 &&
        environmentCode >= 0 && environmentCode <= 4) {
        return environmentCode;
    }
    return normalized;
}

[[nodiscard]] constexpr PreviewTextureKey ClassicTextureKeyFor(
    const PreviewSurface& surface) noexcept
{
    const std::int16_t collection =
        NormalizeClassicCollection(surface.texture.collection);
    if (!IsClassicTextureCollection(collection) ||
        surface.texture.bitmap < 0) {
        return PreviewTextureKey{};
    }
    return PreviewTextureKey{
        collection,
        surface.texture.bitmap,
    };
}

[[nodiscard]] constexpr float ClassicSurfaceOpacity(
    SurfaceKind kind) noexcept
{
    return kind == SurfaceKind::Media ? 0.65F : 1.0F;
}

[[nodiscard]] constexpr float ClassicSurfaceOpacity(
    const PreviewSurface& surface) noexcept
{
    // Transparent wall bitmaps carry their own alpha from the Shapes decoder.
    // Keep their material opacity at one and route them through the translucent
    // pass so bitmap alpha, rather than an arbitrary global fade, wins.
    // Delegate to the inherited SurfaceKind overload so TEX-1A callers remain
    // source-compatible while TEX-1A.2 passes full surface provenance.
    return ClassicSurfaceOpacity(surface.id.kind);
}

}  // namespace pfhorge::preview
