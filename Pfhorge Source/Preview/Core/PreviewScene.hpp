// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#include "PreviewTypes.hpp"

#include <cstdint>
#include <vector>

namespace pfhorge::preview {

struct PreviewSurface final {
    SurfaceID id;
    StableID polygonID = kInvalidID;
    StableID sideID = kInvalidID;
    TextureDescriptor texture;
    std::int16_t lightIndex = -1;
    std::vector<PreviewVertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct PreviewPolygon final {
    StableID id = kInvalidID;
    std::vector<StableID> endpointIDs;
    std::vector<StableID> adjacentPolygonIDs;
    float floorHeight = 0.0F;
    float ceilingHeight = 0.0F;
};

struct PreviewScene final {
    std::uint64_t revision = 0;
    std::vector<Vec3> endpoints;
    std::vector<PreviewPolygon> polygons;
    std::vector<PreviewSurface> surfaces;

    [[nodiscard]] bool empty() const noexcept {
        return polygons.empty() && surfaces.empty();
    }
};

}  // namespace pfhorge::preview
