// SPDX-License-Identifier: GPL-3.0-or-later

#include "../Core/PreviewVisibility.hpp"

#include <algorithm>
#include <cassert>

using namespace pfhorge::preview;

static PreviewPolygon Rectangle(
    StableID id,
    StableID first,
    float floor,
    float ceiling)
{
    PreviewPolygon polygon;
    polygon.id = id;
    polygon.endpointIDs = {first, first + 1U, first + 2U, first + 3U};
    polygon.floorHeight = floor;
    polygon.ceilingHeight = ceiling;
    return polygon;
}

int main()
{
    PreviewScene scene;
    scene.endpoints = {
        Vec3{0.0F, 0.0F, 0.0F},
        Vec3{4.0F, 0.0F, 0.0F},
        Vec3{4.0F, 0.0F, 4.0F},
        Vec3{0.0F, 0.0F, 4.0F},

        // Same X/Z footprint, intentionally overlapping in height. The explicit
        // player-start seed must win over first-match containment.
        Vec3{0.0F, 0.0F, 0.0F},
        Vec3{4.0F, 0.0F, 0.0F},
        Vec3{4.0F, 0.0F, 4.0F},
        Vec3{0.0F, 0.0F, 4.0F},

        Vec3{8.0F, 0.0F, 0.0F},
        Vec3{8.0F, 0.0F, 4.0F},
    };

    PreviewPolygon first = Rectangle(0U, 0U, 0.0F, 3.0F);
    PreviewPolygon preferred = Rectangle(1U, 4U, 0.0F, 3.0F);
    PreviewPolygon destination;
    destination.id = 2U;
    destination.endpointIDs = {5U, 8U, 9U, 6U};
    destination.floorHeight = 0.0F;
    destination.ceilingHeight = 3.0F;

    scene.polygons = {first, preferred, destination};
    scene.portals = {
        PreviewPortal{
            1U,
            2U,
            1U,
            Vec3{4.0F, 0.0F, 1.0F},
            Vec3{4.0F, 0.0F, 3.0F},
            Vec3{4.0F, 3.0F, 3.0F},
            Vec3{4.0F, 3.0F, 1.0F},
        },
    };

    const auto interior = FindInteriorPoint(scene, 1U);
    assert(interior.has_value());
    assert(PolygonContainsPoint3D(scene, 1U, *interior));

    PreviewCamera camera;
    camera.position = Vec3{2.0F, 1.5F, 2.0F};
    camera.forward = Vec3{1.0F, 0.0F, 0.0F};
    camera.aspectRatio = 16.0F / 9.0F;

    PreviewTraversalDiagnostics diagnostics;
    const PreviewFrame frame = BuildPortalPreviewFrame(
        scene,
        camera,
        std::optional<StableID>(1U),
        &diagnostics);

    assert(frame.cameraInsideScene);
    assert(frame.cameraPolygonID == 1U);
    assert(diagnostics.preferredSeedRequested);
    assert(diagnostics.preferredSeedAccepted);
    assert(diagnostics.portalsExamined == 1U);
    assert(diagnostics.portalsAccepted == 1U);
    assert(std::find(
        frame.visiblePolygonIDs.begin(),
        frame.visiblePolygonIDs.end(),
        2U) != frame.visiblePolygonIDs.end());

    // A preferred seed that does not contain the camera must be rejected and
    // normal coordinate containment must take over.
    const PreviewFrame fallback = BuildPortalPreviewFrame(
        scene,
        camera,
        std::optional<StableID>(2U),
        &diagnostics);
    assert(fallback.cameraInsideScene);
    assert(fallback.cameraPolygonID == 0U);
    assert(diagnostics.preferredSeedRequested);
    assert(!diagnostics.preferredSeedAccepted);

    return 0;
}
