// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#include <cstdint>
#include <limits>

namespace pfhorge::preview {

using StableID = std::uint32_t;

constexpr StableID kInvalidPreviewID = std::numeric_limits<StableID>::max();

struct Vec2 final {
    float x = 0.0F;
    float y = 0.0F;
};

struct Vec3 final {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct PreviewVertex final {
    Vec3 position;
    Vec2 textureCoordinate;
    float light = 1.0F;
};

enum class SurfaceKind : std::uint8_t {
    Wall,
    Floor,
    Ceiling,
    Media,
    Landscape,
    Sprite,
    EditorOverlay,
};

/**
 * The editable texture slot represented by a preview surface.
 *
 * This is renderer-neutral provenance. TEX-2 surface picking and the
 * companion Visual Mode texture palette will use the same value instead of
 * guessing primary/secondary/transparent state from geometry afterward.
 */
enum class SurfaceTextureLayer : std::uint8_t {
    None,
    Floor,
    Ceiling,
    Primary,
    Secondary,
    Transparent,
    Media,
};

struct TextureDescriptor final {
    std::int16_t collection = -1;
    std::int16_t bitmap = -1;
    std::int16_t transferMode = 0;
};

struct SurfaceID final {
    SurfaceKind kind = SurfaceKind::Wall;
    StableID owner = kInvalidPreviewID;
    std::uint16_t subpart = 0;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return owner != kInvalidPreviewID;
    }
};

}  // namespace pfhorge::preview
