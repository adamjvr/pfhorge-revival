// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#ifdef __OBJC__

#import "LELevelData.h"
#import "LELine.h"
#import "LEMapPoint.h"
#import "LEMapObject.h"
#import "LEPolygon.h"

#include "PreviewScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace pfhorge::preview {

namespace detail {

constexpr float kWorldUnitScale = 1.0F / 1024.0F;

[[nodiscard]] inline Vec3 pointAtHeight(
    LEMapPoint *point,
    short height) noexcept
{
    return Vec3{
        static_cast<float>(point.x) * kWorldUnitScale,
        static_cast<float>(height) * kWorldUnitScale,
        -static_cast<float>(point.y) * kWorldUnitScale,
    };
}

[[nodiscard]] inline Vec2 textureCoordinateForPoint(
    LEMapPoint *point) noexcept
{
    return Vec2{
        static_cast<float>(point.x) * kWorldUnitScale,
        static_cast<float>(point.y) * kWorldUnitScale,
    };
}

inline void appendTriangleFan(
    PreviewSurface& surface,
    bool reverseWinding)
{
    const std::uint32_t vertexCount =
        static_cast<std::uint32_t>(surface.vertices.size());

    if (vertexCount < 3U) {
        return;
    }

    for (std::uint32_t index = 1U;
         index + 1U < vertexCount;
         ++index) {
        surface.indices.push_back(0U);

        if (reverseWinding) {
            surface.indices.push_back(index + 1U);
            surface.indices.push_back(index);
        } else {
            surface.indices.push_back(index);
            surface.indices.push_back(index + 1U);
        }
    }
}

inline void appendWallSegment(
    PreviewScene& scene,
    StableID polygonID,
    std::uint16_t edgeIndex,
    std::uint16_t segmentIndex,
    LEMapPoint *first,
    LEMapPoint *second,
    short lowerHeight,
    short upperHeight)
{
    if (first == nil ||
        second == nil ||
        upperHeight <= lowerHeight) {
        return;
    }

    PreviewSurface wall;
    wall.id = SurfaceID{
        SurfaceKind::Wall,
        polygonID,
        static_cast<std::uint16_t>(
            edgeIndex * 3U + segmentIndex),
    };
    wall.polygonID = polygonID;
    wall.vertices = {
        PreviewVertex{
            pointAtHeight(first, lowerHeight),
            Vec2{0.0F, 0.0F},
            1.0F,
        },
        PreviewVertex{
            pointAtHeight(second, lowerHeight),
            Vec2{1.0F, 0.0F},
            1.0F,
        },
        PreviewVertex{
            pointAtHeight(second, upperHeight),
            Vec2{1.0F, 1.0F},
            1.0F,
        },
        PreviewVertex{
            pointAtHeight(first, upperHeight),
            Vec2{0.0F, 1.0F},
            1.0F,
        },
    };
    wall.indices = {
        0U, 1U, 2U,
        0U, 2U, 3U,
    };

    scene.surfaces.push_back(std::move(wall));
}

[[nodiscard]] inline bool lineIsTransparent(
    LELine *line) noexcept
{
    return line != nil &&
           (line.flags & LELineTransparent) != 0;
}

[[nodiscard]] inline StableID validAdjacentPolygonID(
    LEPolygon *polygon,
    NSUInteger edgeIndex,
    NSArray<LEPolygon *> *polygons) noexcept
{
    if (polygon == nil ||
        edgeIndex >= static_cast<NSUInteger>(
            std::max<short>(0, polygon.getTheVertexCount))) {
        return kInvalidPreviewID;
    }

    LELine *line =
        [polygon lineObjectAtIndex:
            static_cast<short>(edgeIndex)];

    if (!lineIsTransparent(line)) {
        return kInvalidPreviewID;
    }

    const short adjacentIndex =
        [polygon adjacentPolygonIndexesAtIndex:
            static_cast<short>(edgeIndex)];

    if (adjacentIndex < 0 ||
        static_cast<NSUInteger>(adjacentIndex) >= polygons.count) {
        return kInvalidPreviewID;
    }

    return static_cast<StableID>(adjacentIndex);
}

}  // namespace detail

/**
 * Builds an immutable renderer snapshot from the live Objective-C map model.
 *
 * VM-3 records directed transparent portals and splits each source polygon's
 * boundary into lower wall, portal opening, and upper wall regions. Renderer
 * code never retains Objective-C map pointers.
 */
[[nodiscard]] inline PreviewScene BuildPreviewScene(
    LELevelData *levelData)
{
    PreviewScene scene;
    scene.revision = 3U;

    if (levelData == nil) {
        return scene;
    }

    NSArray<LEMapPoint *> *points = [levelData points];
    scene.endpoints.reserve(points.count);

    for (LEMapPoint *point in points) {
        scene.endpoints.push_back(Vec3{
            static_cast<float>(point.x) *
                detail::kWorldUnitScale,
            0.0F,
            -static_cast<float>(point.y) *
                detail::kWorldUnitScale,
        });
    }

    NSArray<LEPolygon *> *polygons = [levelData polygons];
    scene.polygons.reserve(polygons.count);

    for (NSUInteger polygonIndex = 0U;
         polygonIndex < polygons.count;
         ++polygonIndex) {
        LEPolygon *polygon = polygons[polygonIndex];
        NSArray<LEMapPoint *> *vertices =
            polygon.vertexArray;

        const NSUInteger requestedCount =
            static_cast<NSUInteger>(
                std::max<short>(
                    0,
                    polygon.getTheVertexCount));
        const NSUInteger vertexCount =
            std::min(vertices.count, requestedCount);

        if (vertexCount < 3U) {
            continue;
        }

        const StableID stablePolygonID =
            static_cast<StableID>(polygonIndex);

        PreviewPolygon previewPolygon;
        previewPolygon.id = stablePolygonID;
        previewPolygon.floorHeight =
            static_cast<float>(polygon.floorHeight) *
            detail::kWorldUnitScale;
        previewPolygon.ceilingHeight =
            static_cast<float>(polygon.ceilingHeight) *
            detail::kWorldUnitScale;
        previewPolygon.endpointIDs.reserve(vertexCount);
        previewPolygon.adjacentPolygonIDs.reserve(vertexCount);

        for (NSUInteger vertexIndex = 0U;
             vertexIndex < vertexCount;
             ++vertexIndex) {
            LEMapPoint *point = vertices[vertexIndex];
            const NSUInteger endpointIndex =
                [points indexOfObjectIdenticalTo:point];

            previewPolygon.endpointIDs.push_back(
                endpointIndex == NSNotFound
                    ? kInvalidPreviewID
                    : static_cast<StableID>(
                        endpointIndex));

            previewPolygon.adjacentPolygonIDs.push_back(
                detail::validAdjacentPolygonID(
                    polygon,
                    vertexIndex,
                    polygons));
        }

        scene.polygons.push_back(previewPolygon);

        PreviewSurface floor;
        floor.id = SurfaceID{
            SurfaceKind::Floor,
            stablePolygonID,
            0U,
        };
        floor.polygonID = stablePolygonID;
        floor.texture.collection =
            polygon.floorTextureCollectionOnly;
        floor.texture.bitmap =
            polygon.floorTextureOnly;
        floor.texture.transferMode =
            polygon.floorTransferMode;
        floor.lightIndex =
            polygon.floorLightsourceIndex;
        floor.vertices.reserve(vertexCount);

        PreviewSurface ceiling;
        ceiling.id = SurfaceID{
            SurfaceKind::Ceiling,
            stablePolygonID,
            0U,
        };
        ceiling.polygonID = stablePolygonID;
        ceiling.texture.collection =
            polygon.ceilingTextureCollectionOnly;
        ceiling.texture.bitmap =
            polygon.ceilingTextureOnly;
        ceiling.texture.transferMode =
            polygon.ceilingTransferMode;
        ceiling.lightIndex =
            polygon.ceilingLightsourceIndex;
        ceiling.vertices.reserve(vertexCount);

        for (NSUInteger vertexIndex = 0U;
             vertexIndex < vertexCount;
             ++vertexIndex) {
            LEMapPoint *point = vertices[vertexIndex];
            const Vec2 textureCoordinate =
                detail::textureCoordinateForPoint(point);

            floor.vertices.push_back(PreviewVertex{
                detail::pointAtHeight(
                    point,
                    polygon.floorHeight),
                textureCoordinate,
                1.0F,
            });

            ceiling.vertices.push_back(PreviewVertex{
                detail::pointAtHeight(
                    point,
                    polygon.ceilingHeight),
                textureCoordinate,
                1.0F,
            });
        }

        detail::appendTriangleFan(floor, false);
        detail::appendTriangleFan(ceiling, true);

        scene.surfaces.push_back(std::move(floor));
        scene.surfaces.push_back(std::move(ceiling));

        for (NSUInteger edgeIndex = 0U;
             edgeIndex < vertexCount;
             ++edgeIndex) {
            const NSUInteger nextIndex =
                (edgeIndex + 1U) % vertexCount;

            LEMapPoint *first = vertices[edgeIndex];
            LEMapPoint *second = vertices[nextIndex];
            const StableID adjacentID =
                previewPolygon.adjacentPolygonIDs[edgeIndex];

            if (adjacentID == kInvalidPreviewID ||
                adjacentID >= polygons.count) {
                detail::appendWallSegment(
                    scene,
                    stablePolygonID,
                    static_cast<std::uint16_t>(edgeIndex),
                    0U,
                    first,
                    second,
                    polygon.floorHeight,
                    polygon.ceilingHeight);
                continue;
            }

            LEPolygon *adjacentPolygon =
                polygons[static_cast<NSUInteger>(adjacentID)];

            const short openingBottom =
                std::max(
                    polygon.floorHeight,
                    adjacentPolygon.floorHeight);
            const short openingTop =
                std::min(
                    polygon.ceilingHeight,
                    adjacentPolygon.ceilingHeight);

            if (openingTop <= openingBottom) {
                detail::appendWallSegment(
                    scene,
                    stablePolygonID,
                    static_cast<std::uint16_t>(edgeIndex),
                    0U,
                    first,
                    second,
                    polygon.floorHeight,
                    polygon.ceilingHeight);
                continue;
            }

            scene.portals.push_back(PreviewPortal{
                stablePolygonID,
                adjacentID,
                static_cast<std::uint16_t>(edgeIndex),
                detail::pointAtHeight(first, openingBottom),
                detail::pointAtHeight(second, openingBottom),
                detail::pointAtHeight(second, openingTop),
                detail::pointAtHeight(first, openingTop),
            });

            detail::appendWallSegment(
                scene,
                stablePolygonID,
                static_cast<std::uint16_t>(edgeIndex),
                0U,
                first,
                second,
                polygon.floorHeight,
                openingBottom);

            detail::appendWallSegment(
                scene,
                stablePolygonID,
                static_cast<std::uint16_t>(edgeIndex),
                2U,
                first,
                second,
                openingTop,
                polygon.ceilingHeight);
        }
    }


    NSArray<LEMapObject *> *mapObjects = [levelData theMapObjects];
    scene.playerStarts.reserve(mapObjects.count);

    for (NSUInteger objectIndex = 0U;
         objectIndex < mapObjects.count;
         ++objectIndex) {
        LEMapObject *object = mapObjects[objectIndex];
        if (object.type != _saved_player) {
            continue;
        }

        const short polygonIndex = object.polygonIndex;
        if (polygonIndex < 0 ||
            static_cast<NSUInteger>(polygonIndex) >= polygons.count) {
            continue;
        }

        LEPolygon *polygon = polygons[static_cast<NSUInteger>(polygonIndex)];
        const float floorHeight =
            static_cast<float>(polygon.floorHeight) * detail::kWorldUnitScale;
        const float ceilingHeight =
            static_cast<float>(polygon.ceilingHeight) * detail::kWorldUnitScale;
        const float clearance = ceilingHeight - floorHeight;
        if (clearance <= 0.20F) {
            continue;
        }

        // Marathon map-object Z is relative to the owning polygon floor for
        // ordinary floor-standing objects. Add a conservative editor eye height
        // and clamp it inside the current floor/ceiling interval.
        const float requestedEyeHeight =
            floorHeight +
            static_cast<float>(object.z) * detail::kWorldUnitScale +
            0.50F;
        const float eyeHeight = std::clamp(
            requestedEyeHeight,
            floorHeight + 0.05F,
            ceilingHeight - 0.05F);

        constexpr float kTwoPi = 6.28318530717958647692F;
        constexpr float kMarathonAngleUnits = 512.0F;
        const float marathonAngle =
            static_cast<float>(object.facing) *
            kTwoPi / kMarathonAngleUnits;

        PreviewPlayerStart start;
        start.objectID = static_cast<StableID>(objectIndex);
        start.polygonID = static_cast<StableID>(polygonIndex);
        start.position = Vec3{
            static_cast<float>(object.x) * detail::kWorldUnitScale,
            eyeHeight,
            -static_cast<float>(object.y) * detail::kWorldUnitScale,
        };
        // Marathon angle zero points east. The Metal camera uses yaw zero for
        // negative Z, and map Y is inverted into preview Z.
        start.yawRadians =
            1.57079632679489661923F - marathonAngle;
        scene.playerStarts.push_back(start);
    }

    return scene;
}

}  // namespace pfhorge::preview

#endif  // __OBJC__
