// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#include "PreviewScene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pfhorge::preview {

struct PortalClipRegion final {
    float left = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
    float top = 0.0F;

    [[nodiscard]] bool valid(float epsilon = 0.00001F) const noexcept {
        return right - left > epsilon && top - bottom > epsilon;
    }

    [[nodiscard]] bool encloses(
        const PortalClipRegion& other,
        float epsilon = 0.0001F) const noexcept
    {
        return left <= other.left + epsilon &&
               right >= other.right - epsilon &&
               bottom <= other.bottom + epsilon &&
               top >= other.top - epsilon;
    }
};

struct PreviewCamera final {
    Vec3 position;
    Vec3 forward{0.0F, 0.0F, -1.0F};
    float verticalFieldOfViewRadians = 1.0471975512F;
    float aspectRatio = 1.0F;
    float nearPlane = 0.01F;
    std::uint32_t maximumTraversalDepth = 96U;
    std::uint32_t maximumRegionsPerPolygon = 24U;
};

struct PreviewFrame final {
    StableID cameraPolygonID = kInvalidPreviewID;
    bool cameraInsideScene = false;
    std::vector<StableID> visiblePolygonIDs;
    std::vector<PreviewSurface> visibleSurfaces;
    std::vector<PreviewPortal> visiblePortals;
};

struct PreviewTraversalDiagnostics final {
    bool preferredSeedRequested = false;
    bool preferredSeedAccepted = false;
    std::uint32_t portalsExamined = 0U;
    std::uint32_t portalsRejectedClosed = 0U;
    std::uint32_t portalsRejectedProjection = 0U;
    std::uint32_t portalsRejectedClip = 0U;
    std::uint32_t portalsRejectedDuplicate = 0U;
    std::uint32_t portalsAccepted = 0U;
    std::uint32_t traversalDepthStops = 0U;
    std::size_t visiblePolygonCount = 0U;
    std::size_t visibleSurfaceCount = 0U;
};

namespace detail {

[[nodiscard]] inline Vec3 Add(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] inline Vec3 Subtract(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] inline Vec3 Scale(const Vec3& value, float scalar) noexcept {
    return Vec3{
        value.x * scalar,
        value.y * scalar,
        value.z * scalar,
    };
}

[[nodiscard]] inline float Dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] inline Vec3 Cross(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

[[nodiscard]] inline float LengthSquared(const Vec3& value) noexcept {
    return Dot(value, value);
}

[[nodiscard]] inline Vec3 NormalizeOr(
    const Vec3& value,
    const Vec3& fallback) noexcept
{
    const float lengthSquared = LengthSquared(value);

    if (lengthSquared <= 0.0000001F) {
        return fallback;
    }

    return Scale(value, 1.0F / std::sqrt(lengthSquared));
}

[[nodiscard]] inline const PreviewPolygon *FindPolygon(
    const PreviewScene& scene,
    StableID polygonID) noexcept
{
    const auto iterator = std::find_if(
        scene.polygons.begin(),
        scene.polygons.end(),
        [polygonID](const PreviewPolygon& polygon) {
            return polygon.id == polygonID;
        });

    return iterator == scene.polygons.end()
        ? nullptr
        : &*iterator;
}

[[nodiscard]] inline PortalClipRegion Intersect(
    const PortalClipRegion& first,
    const PortalClipRegion& second) noexcept
{
    return PortalClipRegion{
        std::max(first.left, second.left),
        std::min(first.right, second.right),
        std::max(first.bottom, second.bottom),
        std::min(first.top, second.top),
    };
}

[[nodiscard]] inline std::optional<PortalClipRegion> ProjectPortal(
    const PreviewPortal& portal,
    const PreviewCamera& camera) noexcept
{
    const Vec3 forward =
        NormalizeOr(camera.forward, Vec3{0.0F, 0.0F, -1.0F});
    const Vec3 worldUp{0.0F, 1.0F, 0.0F};
    const Vec3 right =
        NormalizeOr(Cross(forward, worldUp), Vec3{1.0F, 0.0F, 0.0F});
    const Vec3 cameraUp = NormalizeOr(Cross(right, forward), worldUp);

    struct CameraPoint final {
        float x;
        float y;
        float depth;
    };

    const std::array<Vec3, 4U> corners{
        portal.lowerLeft,
        portal.lowerRight,
        portal.upperRight,
        portal.upperLeft,
    };

    std::vector<CameraPoint> polygon;
    polygon.reserve(6U);
    for (const Vec3& corner : corners) {
        const Vec3 relative = Subtract(corner, camera.position);
        polygon.push_back(CameraPoint{
            Dot(relative, right),
            Dot(relative, cameraUp),
            Dot(relative, forward),
        });
    }

    // Clip the portal quad against the near plane before perspective division.
    // Ignoring behind-camera corners produces collapsed rectangles when the
    // camera approaches or crosses a real imported portal.
    std::vector<CameraPoint> clipped;
    clipped.reserve(8U);
    for (std::size_t current = 0U; current < polygon.size(); ++current) {
        const CameraPoint& a = polygon[current];
        const CameraPoint& b = polygon[(current + 1U) % polygon.size()];
        const bool aInside = a.depth >= camera.nearPlane;
        const bool bInside = b.depth >= camera.nearPlane;

        if (aInside) {
            clipped.push_back(a);
        }

        if (aInside != bInside) {
            const float denominator = b.depth - a.depth;
            if (std::fabs(denominator) > 0.0000001F) {
                const float t =
                    (camera.nearPlane - a.depth) / denominator;
                clipped.push_back(CameraPoint{
                    a.x + (b.x - a.x) * t,
                    a.y + (b.y - a.y) * t,
                    camera.nearPlane,
                });
            }
        }
    }

    if (clipped.size() < 3U) {
        return std::nullopt;
    }

    float minimumX = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    float minimumY = std::numeric_limits<float>::max();
    float maximumY = std::numeric_limits<float>::lowest();

    for (const CameraPoint& point : clipped) {
        const float safeDepth = std::max(point.depth, camera.nearPlane);
        minimumX = std::min(minimumX, point.x / safeDepth);
        maximumX = std::max(maximumX, point.x / safeDepth);
        minimumY = std::min(minimumY, point.y / safeDepth);
        maximumY = std::max(maximumY, point.y / safeDepth);
    }

    PortalClipRegion result{minimumX, maximumX, minimumY, maximumY};
    return result.valid() ? std::optional<PortalClipRegion>(result) : std::nullopt;
}

inline bool RecordRegionIfNew(
    std::unordered_map<StableID, std::vector<PortalClipRegion>>& regions,
    StableID polygonID,
    const PortalClipRegion& candidate,
    std::uint32_t maximumRegionsPerPolygon)
{
    std::vector<PortalClipRegion>& existing = regions[polygonID];

    for (const PortalClipRegion& region : existing) {
        if (region.encloses(candidate)) {
            return false;
        }
    }

    existing.erase(
        std::remove_if(
            existing.begin(),
            existing.end(),
            [&candidate](const PortalClipRegion& region) {
                return candidate.encloses(region);
            }),
        existing.end());

    if (existing.size() >= maximumRegionsPerPolygon) {
        return false;
    }

    existing.push_back(candidate);
    return true;
}

}  // namespace detail

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

        const float denominator = b.z - a.z;
        const bool crosses =
            ((a.z > point.z) != (b.z > point.z)) &&
            (point.x <
             (b.x - a.x) * (point.z - a.z) /
                 (std::fabs(denominator) < 0.000001F
                      ? 0.000001F
                      : denominator) +
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

[[nodiscard]] inline bool PolygonContainsPoint3D(
    const PreviewScene& scene,
    StableID polygonID,
    const Vec3& point) noexcept
{
    const PreviewPolygon *polygon = detail::FindPolygon(scene, polygonID);
    return polygon != nullptr &&
           point.y >= polygon->floorHeight &&
           point.y <= polygon->ceilingHeight &&
           PointInsidePolygonXZ(scene, *polygon, point);
}

[[nodiscard]] inline std::optional<Vec3> FindInteriorPoint(
    const PreviewScene& scene,
    StableID polygonID) noexcept
{
    const PreviewPolygon *polygon = detail::FindPolygon(scene, polygonID);
    if (polygon == nullptr || polygon->endpointIDs.size() < 3U) {
        return std::nullopt;
    }

    Vec3 centroid{0.0F, (polygon->floorHeight + polygon->ceilingHeight) * 0.5F, 0.0F};
    std::size_t count = 0U;
    for (StableID endpointID : polygon->endpointIDs) {
        if (endpointID >= scene.endpoints.size()) {
            continue;
        }
        centroid.x += scene.endpoints[endpointID].x;
        centroid.z += scene.endpoints[endpointID].z;
        ++count;
    }
    if (count == 0U) {
        return std::nullopt;
    }
    centroid.x /= static_cast<float>(count);
    centroid.z /= static_cast<float>(count);
    if (PointInsidePolygonXZ(scene, *polygon, centroid)) {
        return centroid;
    }

    const StableID firstID = polygon->endpointIDs.front();
    if (firstID >= scene.endpoints.size()) {
        return std::nullopt;
    }
    const Vec3& first = scene.endpoints[firstID];
    for (std::size_t index = 1U; index + 1U < polygon->endpointIDs.size(); ++index) {
        const StableID secondID = polygon->endpointIDs[index];
        const StableID thirdID = polygon->endpointIDs[index + 1U];
        if (secondID >= scene.endpoints.size() || thirdID >= scene.endpoints.size()) {
            continue;
        }
        const Vec3& second = scene.endpoints[secondID];
        const Vec3& third = scene.endpoints[thirdID];
        Vec3 candidate{
            (first.x + second.x + third.x) / 3.0F,
            centroid.y,
            (first.z + second.z + third.z) / 3.0F,
        };
        if (PointInsidePolygonXZ(scene, *polygon, candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

[[nodiscard]] inline PreviewFrame BuildWholeScenePreviewFrame(
    const PreviewScene& scene)
{
    PreviewFrame frame;
    frame.visibleSurfaces = scene.surfaces;
    frame.visiblePortals = scene.portals;
    frame.visiblePolygonIDs.reserve(scene.polygons.size());

    for (const PreviewPolygon& polygon : scene.polygons) {
        frame.visiblePolygonIDs.push_back(polygon.id);
    }

    return frame;
}

/**
 * Builds a renderer-neutral frame using projected portal clipping.
 *
 * The traversal follows the same core invariants used by Marathon/Aleph One:
 * start in the camera polygon, cross only transparent polygon transitions,
 * narrow inherited clipping windows at each portal, and permit a polygon to be
 * revisited when it is reached through a materially different clip region.
 *
 * This is an independently written floating-point adaptation for editor
 * preview use. It does not copy Aleph One's map globals, fixed-point ray
 * caster, automap mutation, object placement, or rasterizer state.
 */
[[nodiscard]] inline PreviewFrame BuildPortalPreviewFrame(
    const PreviewScene& scene,
    const PreviewCamera& camera,
    std::optional<StableID> preferredSeed,
    PreviewTraversalDiagnostics *diagnostics)
{
    PreviewFrame frame;
    if (diagnostics != nullptr) {
        *diagnostics = PreviewTraversalDiagnostics{};
        diagnostics->preferredSeedRequested = preferredSeed.has_value();
    }

    std::optional<StableID> containing;
    if (preferredSeed.has_value() &&
        PolygonContainsPoint3D(scene, *preferredSeed, camera.position)) {
        containing = preferredSeed;
        if (diagnostics != nullptr) {
            diagnostics->preferredSeedAccepted = true;
        }
    } else {
        containing = FindContainingPolygon(scene, camera.position);
    }

    if (!containing.has_value()) {
        return frame;
    }

    frame.cameraPolygonID = *containing;
    frame.cameraInsideScene = true;

    const float verticalHalfExtent =
        std::tan(
            std::clamp(
                camera.verticalFieldOfViewRadians,
                0.05F,
                3.0F) *
            0.5F);
    const float horizontalHalfExtent =
        verticalHalfExtent *
        std::max(0.01F, camera.aspectRatio);

    const PortalClipRegion initialRegion{
        -horizontalHalfExtent,
        horizontalHalfExtent,
        -verticalHalfExtent,
        verticalHalfExtent,
    };

    struct TraversalNode final {
        StableID polygonID = kInvalidPreviewID;
        PortalClipRegion clipRegion;
        std::uint32_t depth = 0U;
    };

    std::deque<TraversalNode> pending;
    std::unordered_map<StableID, std::vector<PortalClipRegion>>
        visitedRegions;
    std::unordered_set<StableID> visiblePolygons;

    pending.push_back(
        TraversalNode{
            *containing,
            initialRegion,
            0U,
        });
    detail::RecordRegionIfNew(
        visitedRegions,
        *containing,
        initialRegion,
        camera.maximumRegionsPerPolygon);

    while (!pending.empty()) {
        const TraversalNode node = pending.front();
        pending.pop_front();

        if (detail::FindPolygon(scene, node.polygonID) == nullptr) {
            continue;
        }

        visiblePolygons.insert(node.polygonID);

        if (node.depth >= camera.maximumTraversalDepth) {
            if (diagnostics != nullptr) {
                ++diagnostics->traversalDepthStops;
            }
            continue;
        }

        for (const PreviewPortal& portal : scene.portals) {
            if (portal.sourcePolygonID != node.polygonID) {
                continue;
            }
            if (diagnostics != nullptr) {
                ++diagnostics->portalsExamined;
            }
            if (!portal.open()) {
                if (diagnostics != nullptr) {
                    ++diagnostics->portalsRejectedClosed;
                }
                continue;
            }

            const std::optional<PortalClipRegion> projected =
                detail::ProjectPortal(portal, camera);

            if (!projected.has_value()) {
                if (diagnostics != nullptr) {
                    ++diagnostics->portalsRejectedProjection;
                }
                continue;
            }

            const PortalClipRegion clipped =
                detail::Intersect(node.clipRegion, *projected);

            if (!clipped.valid()) {
                if (diagnostics != nullptr) {
                    ++diagnostics->portalsRejectedClip;
                }
                continue;
            }

            if (!detail::RecordRegionIfNew(
                    visitedRegions,
                    portal.destinationPolygonID,
                    clipped,
                    camera.maximumRegionsPerPolygon)) {
                if (diagnostics != nullptr) {
                    ++diagnostics->portalsRejectedDuplicate;
                }
                continue;
            }

            if (diagnostics != nullptr) {
                ++diagnostics->portalsAccepted;
            }
            frame.visiblePortals.push_back(portal);
            pending.push_back(
                TraversalNode{
                    portal.destinationPolygonID,
                    clipped,
                    node.depth + 1U,
                });
        }
    }

    frame.visiblePolygonIDs.assign(
        visiblePolygons.begin(),
        visiblePolygons.end());
    std::sort(
        frame.visiblePolygonIDs.begin(),
        frame.visiblePolygonIDs.end());

    for (const PreviewSurface& surface : scene.surfaces) {
        if (visiblePolygons.count(surface.polygonID) != 0U) {
            frame.visibleSurfaces.push_back(surface);
        }
    }

    if (diagnostics != nullptr) {
        diagnostics->visiblePolygonCount = frame.visiblePolygonIDs.size();
        diagnostics->visibleSurfaceCount = frame.visibleSurfaces.size();
    }
    return frame;
}

[[nodiscard]] inline PreviewFrame BuildPortalPreviewFrame(
    const PreviewScene& scene,
    const PreviewCamera& camera)
{
    return BuildPortalPreviewFrame(scene, camera, std::nullopt, nullptr);
}

/**
 * Compatibility helper retained for the VM-3 foundation smoke test.
 */
[[nodiscard]] inline PreviewFrame BuildTopologicalPreviewFrame(
    const PreviewScene& scene,
    StableID cameraPolygonID)
{
    PreviewFrame frame;
    frame.cameraPolygonID = cameraPolygonID;
    frame.cameraInsideScene =
        detail::FindPolygon(scene, cameraPolygonID) != nullptr;

    std::deque<StableID> pending;
    std::unordered_set<StableID> visited;
    pending.push_back(cameraPolygonID);

    while (!pending.empty()) {
        const StableID polygonID = pending.front();
        pending.pop_front();

        if (!visited.insert(polygonID).second) {
            continue;
        }

        const PreviewPolygon *polygon =
            detail::FindPolygon(scene, polygonID);

        if (polygon == nullptr) {
            continue;
        }

        frame.visiblePolygonIDs.push_back(polygonID);

        for (StableID adjacentID : polygon->adjacentPolygonIDs) {
            if (adjacentID != kInvalidPreviewID) {
                pending.push_back(adjacentID);
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

}  // namespace pfhorge::preview
