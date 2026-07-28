// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#ifdef __OBJC__

#import "LELevelData.h"
#import "LEMapPoint.h"
#import "LEPolygon.h"

#include "PreviewScene.hpp"

#include <algorithm>
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

    for (std::uint32_t index = 1U; index + 1U < vertexCount; ++index) {
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

}  // namespace detail

/**
 * Build the first immutable rendering snapshot from the live Objective-C model.
 *
 * VM-2 deliberately emits every polygon without portal clipping. That is useful
 * for validating the Metal/AppKit integration and map-to-scene conversion.
 * Marathon portal visibility replaces this whole-level emission in VM-3.
 *
 * Concave polygons are still emitted as triangle fans in this prototype. They
 * are therefore diagnostic-only until the surface builder gains the verified
 * Marathon clipping/triangulation path.
 */
[[nodiscard]] inline PreviewScene BuildPreviewScene(
    LELevelData *levelData)
{
    PreviewScene scene;
    scene.revision = 1U;

    if (levelData == nil) {
        return scene;
    }

    NSArray<LEMapPoint *> *points = [levelData points];
    scene.endpoints.reserve(points.count);

    for (LEMapPoint *point in points) {
        scene.endpoints.push_back(Vec3{
            static_cast<float>(point.x) * detail::kWorldUnitScale,
            0.0F,
            -static_cast<float>(point.y) * detail::kWorldUnitScale,
        });
    }

    NSArray<LEPolygon *> *polygons = [levelData polygons];
    scene.polygons.reserve(polygons.count);

    for (NSUInteger polygonIndex = 0U;
         polygonIndex < polygons.count;
         ++polygonIndex) {
        LEPolygon *polygon = polygons[polygonIndex];
        NSArray<LEMapPoint *> *vertices = polygon.vertexArray;

        const NSUInteger requestedCount =
            static_cast<NSUInteger>(std::max<short>(
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

        for (NSUInteger vertexIndex = 0U;
             vertexIndex < vertexCount;
             ++vertexIndex) {
            LEMapPoint *point = vertices[vertexIndex];
            const NSUInteger endpointIndex =
                [points indexOfObjectIdenticalTo:point];

            previewPolygon.endpointIDs.push_back(
                endpointIndex == NSNotFound
                    ? kInvalidPreviewID
                    : static_cast<StableID>(endpointIndex));
        }

        scene.polygons.push_back(std::move(previewPolygon));

        PreviewSurface floor;
        floor.id = SurfaceID{
            SurfaceKind::Floor,
            stablePolygonID,
            0U,
        };
        floor.polygonID = stablePolygonID;
        floor.texture.collection =
            polygon.floorTextureCollectionOnly;
        floor.texture.bitmap = polygon.floorTextureOnly;
        floor.texture.transferMode = polygon.floorTransferMode;
        floor.lightIndex = polygon.floorLightsourceIndex;
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
        ceiling.texture.bitmap = polygon.ceilingTextureOnly;
        ceiling.texture.transferMode =
            polygon.ceilingTransferMode;
        ceiling.lightIndex = polygon.ceilingLightsourceIndex;
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

            PreviewSurface wall;
            wall.id = SurfaceID{
                SurfaceKind::Wall,
                stablePolygonID,
                static_cast<std::uint16_t>(edgeIndex),
            };
            wall.polygonID = stablePolygonID;
            wall.vertices = {
                PreviewVertex{
                    detail::pointAtHeight(
                        first,
                        polygon.floorHeight),
                    Vec2{0.0F, 0.0F},
                    1.0F,
                },
                PreviewVertex{
                    detail::pointAtHeight(
                        second,
                        polygon.floorHeight),
                    Vec2{1.0F, 0.0F},
                    1.0F,
                },
                PreviewVertex{
                    detail::pointAtHeight(
                        second,
                        polygon.ceilingHeight),
                    Vec2{1.0F, 1.0F},
                    1.0F,
                },
                PreviewVertex{
                    detail::pointAtHeight(
                        first,
                        polygon.ceilingHeight),
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
    }

    return scene;
}

}  // namespace pfhorge::preview

#endif  // __OBJC__
