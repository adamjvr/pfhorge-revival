// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#include "PreviewTypes.hpp"

#include <cstdint>
#include <vector>

namespace pfhorge::preview {

struct PreviewSurface final {
    SurfaceID id;
    StableID polygonID = kInvalidPreviewID;
    StableID sideID = kInvalidPreviewID;
    TextureDescriptor texture;
    std::int16_t lightIndex = -1;
    std::vector<PreviewVertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct PreviewPolygon final {
    StableID id = kInvalidPreviewID;
    std::vector<StableID> endpointIDs;
    std::vector<StableID> adjacentPolygonIDs;
    float floorHeight = 0.0F;
    float ceilingHeight = 0.0F;
};

struct PreviewPortal final {
    StableID sourcePolygonID = kInvalidPreviewID;
    StableID destinationPolygonID = kInvalidPreviewID;
    std::uint16_t sourceEdgeIndex = 0U;
    Vec3 lowerLeft;
    Vec3 lowerRight;
    Vec3 upperRight;
    Vec3 upperLeft;

    [[nodiscard]] bool open() const noexcept {
        return destinationPolygonID != kInvalidPreviewID &&
               upperLeft.y > lowerLeft.y &&
               upperRight.y > lowerRight.y;
    }
};

struct PreviewScene final {
    std::uint64_t revision = 0;
    std::vector<Vec3> endpoints;
    std::vector<PreviewPolygon> polygons;
    std::vector<PreviewSurface> surfaces;
    std::vector<PreviewPortal> portals;

    [[nodiscard]] bool empty() const noexcept {
        return polygons.empty() && surfaces.empty();
    }
};

}  // namespace pfhorge::preview
