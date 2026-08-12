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
    StableID lineID = kInvalidPreviewID;
    StableID sideID = kInvalidPreviewID;
    std::uint16_t edgeIndex = 0U;
    SurfaceTextureLayer textureLayer = SurfaceTextureLayer::None;
    TextureDescriptor texture;
    std::int16_t lightIndex = -1;
    bool translucent = false;
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

struct PreviewPlayerStart final {
    StableID objectID = kInvalidPreviewID;
    StableID polygonID = kInvalidPreviewID;
    Vec3 position;
    float yawRadians = 0.0F;
};

/**
 * Renderer-neutral description of a Marathon platform.
 *
 * `initialExtension` is zero for the contracted/open position and one for the
 * extended position. For a door, one generally means closed. The Metal
 * renderer owns temporary animation state; the editor model is never mutated
 * merely because the user opens a door in Visual Mode.
 */
struct PreviewPlatform final {
    StableID id = kInvalidPreviewID;
    StableID polygonID = kInvalidPreviewID;
    std::int16_t type = 0;
    float minimumHeight = 0.0F;
    float maximumHeight = 0.0F;
    float speedWorldUnitsPerSecond = 0.5F;
    float initialExtension = 0.0F;
    Vec3 center;
    bool comesFromFloor = false;
    bool comesFromCeiling = false;
    bool isDoor = false;
    bool playerControllable = false;
    bool locked = false;
};

/** Counts produced while converting the editable map into PreviewScene. */
struct PreviewTextureAudit final {
    std::uint32_t totalReferences = 0U;
    std::uint32_t validReferences = 0U;
    std::uint32_t emptyReferences = 0U;
    std::uint32_t invalidReferences = 0U;
    std::uint32_t remappedReferences = 0U;

    // One resolution attempt is recorded for every polygon edge considered by
    // the surface builder. These counters distinguish map-linkage failures
    // from valid surfaces whose texture image later fails to load.
    std::uint32_t sideResolutionAttempts = 0U;
    std::uint32_t sidesResolvedClockwise = 0U;
    std::uint32_t sidesResolvedCounterclockwise = 0U;
    std::uint32_t sidesResolvedDirect = 0U;
    std::uint32_t sidesResolvedBackReference = 0U;
    std::uint32_t sidesResolvedScan = 0U;
    std::uint32_t rejectedStalePolygonSides = 0U;
    std::uint32_t sideResolutionMisses = 0U;

    std::uint32_t wallSegments = 0U;
    std::uint32_t wallSegmentsWithSide = 0U;
    std::uint32_t wallSegmentsWithoutSide = 0U;
    std::uint32_t wallSegmentsWithTexture = 0U;
    std::uint32_t wallSegmentsWithoutTexture = 0U;
    std::uint32_t compositeSides = 0U;
    std::uint32_t transparentSideReferences = 0U;
    std::uint32_t transparentWallSegments = 0U;
};


struct PreviewTopologyAudit final {
    std::uint32_t polygonEdges = 0U;
    std::uint32_t transparentEdges = 0U;
    std::uint32_t adjacencyResolvedClockwiseOwner = 0U;
    std::uint32_t adjacencyResolvedCounterclockwiseOwner = 0U;
    std::uint32_t adjacencyResolvedDirect = 0U;
    std::uint32_t adjacencyResolutionMisses = 0U;
    std::uint32_t rejectedSelfAdjacency = 0U;
};

struct PreviewScene final {
    std::uint64_t revision = 0;
    std::int16_t environmentCode = 0;
    std::vector<Vec3> endpoints;
    std::vector<PreviewPolygon> polygons;
    std::vector<PreviewSurface> surfaces;
    std::vector<PreviewPortal> portals;
    std::vector<PreviewPlayerStart> playerStarts;
    std::vector<PreviewPlatform> platforms;
    PreviewTextureAudit textureAudit;
    PreviewTopologyAudit topologyAudit;

    [[nodiscard]] bool empty() const noexcept {
        return polygons.empty() && surfaces.empty();
    }
};

}  // namespace pfhorge::preview
