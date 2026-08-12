// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#include "PreviewScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

namespace pfhorge::preview {

struct PreviewMovementResult final {
    Vec3 position;
    StableID polygonID = kInvalidPreviewID;
    bool collided = false;
    bool crossedPortal = false;
};

[[nodiscard]] inline const PreviewPolygon *FindPreviewPolygon(
    const PreviewScene& scene,
    StableID polygonID) noexcept
{
    if (polygonID < scene.polygons.size() &&
        scene.polygons[polygonID].id == polygonID) {
        return &scene.polygons[polygonID];
    }
    for (const PreviewPolygon& polygon : scene.polygons) {
        if (polygon.id == polygonID) {
            return &polygon;
        }
    }
    return nullptr;
}

[[nodiscard]] inline bool PreviewPointInsidePolygon2D(
    const PreviewScene& scene,
    const PreviewPolygon& polygon,
    const Vec3& point) noexcept
{
    const std::size_t count = polygon.endpointIDs.size();
    if (count < 3U) {
        return false;
    }

    bool inside = false;
    for (std::size_t index = 0U, previous = count - 1U;
         index < count;
         previous = index++) {
        const StableID currentID = polygon.endpointIDs[index];
        const StableID previousID = polygon.endpointIDs[previous];
        if (currentID >= scene.endpoints.size() ||
            previousID >= scene.endpoints.size()) {
            return false;
        }

        const Vec3& current = scene.endpoints[currentID];
        const Vec3& prior = scene.endpoints[previousID];
        const bool crosses =
            ((current.z > point.z) != (prior.z > point.z)) &&
            (point.x <
             (prior.x - current.x) *
                     (point.z - current.z) /
                     ((prior.z - current.z) + 0.0000001F) +
                 current.x);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

[[nodiscard]] inline bool PreviewPointFitsVertically(
    const PreviewPolygon& polygon,
    const Vec3& point,
    float margin = 0.05F) noexcept
{
    return point.y >= polygon.floorHeight + margin &&
           point.y <= polygon.ceilingHeight - margin;
}

[[nodiscard]] inline bool PreviewPortalAllowsPoint(
    const PreviewPortal& portal,
    const Vec3& point,
    float margin = 0.05F) noexcept
{
    if (!portal.open()) {
        return false;
    }
    const float bottom = std::max(
        portal.lowerLeft.y,
        portal.lowerRight.y);
    const float top = std::min(
        portal.upperLeft.y,
        portal.upperRight.y);
    return point.y >= bottom + margin &&
           point.y <= top - margin;
}

[[nodiscard]] inline std::optional<StableID> PreviewResolvePosition(
    const PreviewScene& scene,
    StableID currentPolygonID,
    const Vec3& desired,
    float verticalMargin = 0.05F) noexcept
{
    if (const PreviewPolygon *current =
            FindPreviewPolygon(scene, currentPolygonID)) {
        if (PreviewPointInsidePolygon2D(scene, *current, desired) &&
            PreviewPointFitsVertically(*current, desired, verticalMargin)) {
            return currentPolygonID;
        }
    }

    for (const PreviewPortal& portal : scene.portals) {
        if (portal.sourcePolygonID != currentPolygonID ||
            !PreviewPortalAllowsPoint(portal, desired, verticalMargin)) {
            continue;
        }
        const PreviewPolygon *destination =
            FindPreviewPolygon(scene, portal.destinationPolygonID);
        if (destination != nullptr &&
            PreviewPointInsidePolygon2D(scene, *destination, desired) &&
            PreviewPointFitsVertically(
                *destination,
                desired,
                verticalMargin)) {
            return portal.destinationPolygonID;
        }
    }
    return std::nullopt;
}

/**
 * Moves a first-person camera through directed Marathon portals while
 * rejecting solid walls and closed platform openings. The horizontal path is
 * subdivided to avoid tunneling, then axis-separated fallback provides simple
 * wall sliding rather than stopping the camera dead on shallow contact.
 */
[[nodiscard]] inline PreviewMovementResult MovePreviewCameraWithCollision(
    const PreviewScene& scene,
    StableID startingPolygonID,
    const Vec3& startingPosition,
    const Vec3& delta,
    float verticalMargin = 0.05F) noexcept
{
    PreviewMovementResult result;
    result.position = startingPosition;
    result.polygonID = startingPolygonID;

    const float horizontalLength =
        std::sqrt((delta.x * delta.x) + (delta.z * delta.z));
    const int steps = std::clamp(
        static_cast<int>(std::ceil(horizontalLength / 0.08F)),
        1,
        32);
    const Vec3 step{
        delta.x / static_cast<float>(steps),
        0.0F,
        delta.z / static_cast<float>(steps),
    };

    for (int index = 0; index < steps; ++index) {
        const Vec3 full{
            result.position.x + step.x,
            result.position.y,
            result.position.z + step.z,
        };
        if (const auto polygon = PreviewResolvePosition(
                scene,
                result.polygonID,
                full,
                verticalMargin)) {
            result.crossedPortal =
                result.crossedPortal || *polygon != result.polygonID;
            result.position = full;
            result.polygonID = *polygon;
            continue;
        }

        bool movedOnAxis = false;
        const Vec3 xOnly{
            result.position.x + step.x,
            result.position.y,
            result.position.z,
        };
        if (std::fabs(step.x) > 0.000001F) {
            if (const auto polygon = PreviewResolvePosition(
                    scene,
                    result.polygonID,
                    xOnly,
                    verticalMargin)) {
                result.crossedPortal =
                    result.crossedPortal || *polygon != result.polygonID;
                result.position = xOnly;
                result.polygonID = *polygon;
                movedOnAxis = true;
            }
        }

        const Vec3 zOnly{
            result.position.x,
            result.position.y,
            result.position.z + step.z,
        };
        if (std::fabs(step.z) > 0.000001F) {
            if (const auto polygon = PreviewResolvePosition(
                    scene,
                    result.polygonID,
                    zOnly,
                    verticalMargin)) {
                result.crossedPortal =
                    result.crossedPortal || *polygon != result.polygonID;
                result.position = zOnly;
                result.polygonID = *polygon;
                movedOnAxis = true;
            }
        }

        if (!movedOnAxis) {
            result.collided = true;
            break;
        }
        result.collided = true;
    }

    if (const PreviewPolygon *polygon =
            FindPreviewPolygon(scene, result.polygonID)) {
        result.position.y = std::clamp(
            result.position.y + delta.y,
            polygon->floorHeight + verticalMargin,
            polygon->ceilingHeight - verticalMargin);
    }
    return result;
}

[[nodiscard]] inline bool PreviewPolygonsAreDirectlyRelated(
    const PreviewScene& scene,
    StableID first,
    StableID second) noexcept
{
    if (first == second) {
        return true;
    }
    const PreviewPolygon *polygon = FindPreviewPolygon(scene, first);
    if (polygon == nullptr) {
        return false;
    }
    return std::find(
               polygon->adjacentPolygonIDs.begin(),
               polygon->adjacentPolygonIDs.end(),
               second) != polygon->adjacentPolygonIDs.end();
}

/** Returns the best usable door/platform in front of the camera. */
[[nodiscard]] inline std::optional<StableID> FindPreviewDoorForInteraction(
    const PreviewScene& scene,
    StableID cameraPolygonID,
    const Vec3& cameraPosition,
    const Vec3& cameraForward,
    float maximumDistance = 2.5F) noexcept
{
    float bestScore = std::numeric_limits<float>::max();
    std::optional<StableID> best;

    const float forwardLength = std::sqrt(
        cameraForward.x * cameraForward.x +
        cameraForward.z * cameraForward.z);
    const float forwardX = forwardLength > 0.00001F
        ? cameraForward.x / forwardLength
        : 0.0F;
    const float forwardZ = forwardLength > 0.00001F
        ? cameraForward.z / forwardLength
        : -1.0F;

    for (const PreviewPlatform& platform : scene.platforms) {
        if (!platform.isDoor || !platform.playerControllable ||
            !PreviewPolygonsAreDirectlyRelated(
                scene,
                cameraPolygonID,
                platform.polygonID)) {
            continue;
        }
        const float dx = platform.center.x - cameraPosition.x;
        const float dz = platform.center.z - cameraPosition.z;
        const float distance = std::sqrt((dx * dx) + (dz * dz));
        if (distance > maximumDistance) {
            continue;
        }
        const float directionX = distance > 0.00001F ? dx / distance : 0.0F;
        const float directionZ = distance > 0.00001F ? dz / distance : 0.0F;
        const float facing = directionX * forwardX + directionZ * forwardZ;
        if (facing < 0.25F) {
            continue;
        }
        const float score = distance / std::max(0.25F, facing);
        if (score < bestScore) {
            bestScore = score;
            best = platform.id;
        }
    }
    return best;
}

}  // namespace pfhorge::preview
