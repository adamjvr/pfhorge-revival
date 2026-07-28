// SPDX-License-Identifier: GPL-3.0-or-later

#include "../Core/PreviewVisibility.hpp"

#include <cassert>

int main()
{
    using namespace pfhorge::preview;

    PreviewScene scene;
    scene.endpoints = {
        Vec3{0.0F, 0.0F, 0.0F},
        Vec3{4.0F, 0.0F, 0.0F},
        Vec3{4.0F, 0.0F, 4.0F},
        Vec3{0.0F, 0.0F, 4.0F},
        Vec3{8.0F, 0.0F, 0.0F},
        Vec3{8.0F, 0.0F, 4.0F},
    };

    PreviewPolygon first;
    first.id = 0U;
    first.endpointIDs = {0U, 1U, 2U, 3U};
    first.adjacentPolygonIDs = {1U};
    first.floorHeight = 0.0F;
    first.ceilingHeight = 2.0F;

    PreviewPolygon second;
    second.id = 1U;
    second.endpointIDs = {1U, 4U, 5U, 2U};
    second.adjacentPolygonIDs = {0U};
    second.floorHeight = 0.0F;
    second.ceilingHeight = 2.0F;

    scene.polygons = {first, second};

    const auto containing =
        FindContainingPolygon(scene, Vec3{2.0F, 1.0F, 2.0F});
    assert(containing.has_value());
    assert(*containing == 0U);

    const PreviewFrame frame =
        BuildTopologicalPreviewFrame(scene, *containing);
    assert(frame.visiblePolygonIDs.size() == 2U);

    return 0;
}
