// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PreviewScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <queue>
#include <unordered_set>
#include <vector>

namespace pfhorge {
namespace preview {

struct PreviewPortal final {
    StableID sourcePolygonID = kInvalidPreviewID;
    StableID destinationPolygonID = kInvalidPreviewID;
    Vec3 lowerLeft;
    Vec3 lowerRight;
    Vec3 upperRight;
    Vec3 upperLeft;

    [[nodiscard]] bool open() const noexcept {
        return upperLeft.y > lowerLeft.y &&
               upperRight.y > lowerRight.y;
    }
};

struct PreviewFrame final {
    StableID cameraPolygonID = kInvalidPreviewID;
    std::vector<StableID> visiblePolygonIDs;
    std::vector<PreviewSurface> visibleSurfaces;
    std::vector<PreviewPortal> visiblePortals;
};

[[nodiscard]] inline bool PointInsidePolygonXZ(
    const PreviewScene& scene,
    const PreviewPolygon& polygon,
    const Vec3& point) noexcept
{
    if (polygon.endpointIDs.size() < 3U) {
        return false;
    }

    bool inside = false;
    std::size_t previous = polygon.endpointIDs.size() - 1U;

    for (std::size_t current = 0U;
         current < polygon.endpointIDs.size();
         previous = current++) {
        const StableID currentID = polygon.endpointIDs[current];
        const StableID previousID = polygon.endpointIDs[previous];

        if (currentID >= scene.endpoints.size() ||
            previousID >= scene.endpoints.size()) {
            continue;
        }

        const Vec3& a = scene.endpoints[currentID];
        const Vec3& b = scene.endpoints[previousID];

        const bool crosses =
            ((a.z > point.z) != (b.z > point.z)) &&
            (point.x <
             (b.x - a.x) * (point.z - a.z) /
                 ((b.z - a.z) == 0.0F ? 0.000001F : (b.z - a.z)) +
             a.x);

        if (crosses) {
            inside = !inside;
        }
    }

    return inside;
}

[[nodiscard]] inline std::optional<StableID> FindContainingPolygon(
    const PreviewScene& scene,
    const Vec3& cameraPosition) noexcept
{
    for (const PreviewPolygon& polygon : scene.polygons) {
        if (cameraPosition.y < polygon.floorHeight ||
            cameraPosition.y > polygon.ceilingHeight) {
            continue;
        }

        if (PointInsidePolygonXZ(scene, polygon, cameraPosition)) {
            return polygon.id;
        }
    }

    return std::nullopt;
}

/**
 * VM-3 foundation traversal.
 *
 * This deliberately performs topological adjacency traversal only. Screen-space
 * portal clipping and Aleph One-compatible revisit rules are the next VM-3
 * increment. Keeping this stage renderer-neutral lets us test map topology
 * before introducing projection math.
 */
[[nodiscard]] inline PreviewFrame BuildTopologicalPreviewFrame(
    const PreviewScene& scene,
    StableID cameraPolygonID)
{
    PreviewFrame frame;
    frame.cameraPolygonID = cameraPolygonID;

    std::queue<StableID> pending;
    std::unordered_set<StableID> visited;
    pending.push(cameraPolygonID);

    while (!pending.empty()) {
        const StableID polygonID = pending.front();
        pending.pop();

        if (!visited.insert(polygonID).second) {
            continue;
        }

        auto polygonIt = std::find_if(
            scene.polygons.begin(),
            scene.polygons.end(),
            [polygonID](const PreviewPolygon& polygon) {
                return polygon.id == polygonID;
            });

        if (polygonIt == scene.polygons.end()) {
            continue;
        }

        frame.visiblePolygonIDs.push_back(polygonID);

        for (StableID adjacentID : polygonIt->adjacentPolygonIDs) {
            if (adjacentID != kInvalidPreviewID) {
                pending.push(adjacentID);
            }
        }
    }

    for (const PreviewSurface& surface : scene.surfaces) {
        if (visited.count(surface.polygonID) != 0U) {
            frame.visibleSurfaces.push_back(surface);
        }
    }

    return frame;
}

}  // namespace preview
}  // namespace pfhorge
