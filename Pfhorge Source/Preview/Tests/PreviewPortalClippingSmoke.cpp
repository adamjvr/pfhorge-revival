// SPDX-License-Identifier: GPL-3.0-or-later

#include "../Core/PreviewVisibility.hpp"

#include <cassert>

namespace {

using namespace pfhorge::preview;

PreviewPolygon MakeRectangle(
    StableID id,
    StableID a,
    StableID b,
    StableID c,
    StableID d,
    float floor,
    float ceiling)
{
    PreviewPolygon polygon;
    polygon.id = id;
    polygon.endpointIDs = {a, b, c, d};
    polygon.floorHeight = floor;
    polygon.ceilingHeight = ceiling;
    return polygon;
}

PreviewPortal MakePortal(
    StableID source,
    StableID destination,
    float x,
    float z0,
    float z1,
    float bottom,
    float top)
{
    return PreviewPortal{
        source,
        destination,
        0U,
        Vec3{x, bottom, z0},
        Vec3{x, bottom, z1},
        Vec3{x, top, z1},
        Vec3{x, top, z0},
    };
}

}  // namespace

int main()
{
    using namespace pfhorge::preview;

    PreviewScene scene;
    scene.endpoints = {
        Vec3{0.0F, 0.0F, -2.0F},
        Vec3{4.0F, 0.0F, -2.0F},
        Vec3{4.0F, 0.0F, 2.0F},
        Vec3{0.0F, 0.0F, 2.0F},

        Vec3{8.0F, 0.0F, -2.0F},
        Vec3{8.0F, 0.0F, 2.0F},

        Vec3{12.0F, 0.0F, -2.0F},
        Vec3{12.0F, 0.0F, 2.0F},

        // Disconnected polygon occupying the same X/Z bounds as polygon 0.
        Vec3{0.0F, 0.0F, -2.0F},
        Vec3{4.0F, 0.0F, -2.0F},
        Vec3{4.0F, 0.0F, 2.0F},
        Vec3{0.0F, 0.0F, 2.0F},
    };

    PreviewPolygon first =
        MakeRectangle(0U, 0U, 1U, 2U, 3U, 0.0F, 3.0F);
    PreviewPolygon second =
        MakeRectangle(1U, 1U, 4U, 5U, 2U, 0.0F, 3.0F);
    PreviewPolygon third =
        MakeRectangle(2U, 4U, 6U, 7U, 5U, 0.0F, 3.0F);
    PreviewPolygon disconnected =
        MakeRectangle(3U, 8U, 9U, 10U, 11U, 4.0F, 7.0F);

    first.adjacentPolygonIDs = {kInvalidPreviewID, 1U};
    second.adjacentPolygonIDs = {0U, 2U};
    third.adjacentPolygonIDs = {1U};

    scene.polygons = {
        first,
        second,
        third,
        disconnected,
    };

    scene.portals = {
        MakePortal(0U, 1U, 4.0F, -1.0F, 1.0F, 0.0F, 3.0F),
        MakePortal(1U, 0U, 4.0F, 1.0F, -1.0F, 0.0F, 3.0F),
        MakePortal(1U, 2U, 8.0F, -0.75F, 0.75F, 0.25F, 2.75F),
        MakePortal(2U, 1U, 8.0F, 0.75F, -0.75F, 0.25F, 2.75F),
    };

    PreviewCamera camera;
    camera.position = Vec3{2.0F, 1.5F, 0.0F};
    camera.forward = Vec3{1.0F, 0.0F, 0.0F};
    camera.aspectRatio = 16.0F / 9.0F;

    const PreviewFrame frame =
        BuildPortalPreviewFrame(scene, camera);

    assert(frame.cameraInsideScene);
    assert(frame.cameraPolygonID == 0U);
    assert(frame.visiblePolygonIDs.size() == 3U);
    assert(
        std::find(
            frame.visiblePolygonIDs.begin(),
            frame.visiblePolygonIDs.end(),
            3U) == frame.visiblePolygonIDs.end());

    camera.forward = Vec3{-1.0F, 0.0F, 0.0F};

    const PreviewFrame facingAway =
        BuildPortalPreviewFrame(scene, camera);

    assert(facingAway.cameraInsideScene);
    assert(facingAway.visiblePolygonIDs.size() == 1U);
    assert(facingAway.visiblePolygonIDs.front() == 0U);

    camera.position = Vec3{20.0F, 1.5F, 0.0F};
    camera.forward = Vec3{-1.0F, 0.0F, 0.0F};

    const PreviewFrame outside =
        BuildPortalPreviewFrame(scene, camera);

    assert(!outside.cameraInsideScene);
    assert(outside.visiblePolygonIDs.empty());

    PreviewPortal closed =
        MakePortal(0U, 1U, 4.0F, -1.0F, 1.0F, 2.0F, 2.0F);
    assert(!closed.open());

    return 0;
}
