// SPDX-License-Identifier: GPL-3.0-or-later

#include "../Core/PreviewCollision.hpp"
#include "../Core/PreviewTexture.hpp"

#include <cassert>
#include <cmath>

using namespace pfhorge::preview;

static PreviewScene MakeTwoRoomScene()
{
    PreviewScene scene;
    scene.endpoints = {
        {-1.0F, 0.0F, -1.0F},
        { 0.0F, 0.0F, -1.0F},
        { 0.0F, 0.0F,  1.0F},
        {-1.0F, 0.0F,  1.0F},
        { 1.0F, 0.0F, -1.0F},
        { 1.0F, 0.0F,  1.0F},
    };
    PreviewPolygon left;
    left.id = 0U;
    left.endpointIDs = {0U, 1U, 2U, 3U};
    left.adjacentPolygonIDs = {
        kInvalidPreviewID, 1U, kInvalidPreviewID, kInvalidPreviewID,
    };
    left.floorHeight = 0.0F;
    left.ceilingHeight = 2.0F;
    PreviewPolygon right;
    right.id = 1U;
    right.endpointIDs = {1U, 4U, 5U, 2U};
    right.adjacentPolygonIDs = {
        kInvalidPreviewID, kInvalidPreviewID, kInvalidPreviewID, 0U,
    };
    right.floorHeight = 0.0F;
    right.ceilingHeight = 2.0F;
    scene.polygons = {left, right};
    scene.portals.push_back(PreviewPortal{
        0U, 1U, 1U,
        {0.0F, 0.0F, -1.0F},
        {0.0F, 0.0F,  1.0F},
        {0.0F, 2.0F,  1.0F},
        {0.0F, 2.0F, -1.0F},
    });
    scene.portals.push_back(PreviewPortal{
        1U, 0U, 3U,
        {0.0F, 0.0F,  1.0F},
        {0.0F, 0.0F, -1.0F},
        {0.0F, 2.0F, -1.0F},
        {0.0F, 2.0F,  1.0F},
    });
    PreviewPlatform door;
    door.id = 0U;
    door.polygonID = 1U;
    door.center = {0.5F, 1.0F, 0.0F};
    door.isDoor = true;
    door.playerControllable = true;
    scene.platforms.push_back(door);
    return scene;
}

int main()
{
    PreviewScene scene = MakeTwoRoomScene();

    const PreviewMovementResult through = MovePreviewCameraWithCollision(
        scene,
        0U,
        {-0.25F, 1.0F, 0.0F},
        {0.75F, 0.0F, 0.0F});
    assert(through.polygonID == 1U);
    assert(through.crossedPortal);

    PreviewScene closed = scene;
    closed.portals.clear();
    const PreviewMovementResult blocked = MovePreviewCameraWithCollision(
        closed,
        0U,
        {-0.25F, 1.0F, 0.0F},
        {0.75F, 0.0F, 0.0F});
    assert(blocked.polygonID == 0U);
    assert(blocked.collided);
    assert(blocked.position.x <= 0.001F);

    // A diagonal move into the north wall should preserve the valid X motion
    // rather than stopping completely. This is the axis-separated wall-slide
    // behavior used by collision-aware Visual Mode.
    const PreviewMovementResult sliding = MovePreviewCameraWithCollision(
        scene,
        0U,
        {-0.50F, 1.0F, 0.92F},
        {0.35F, 0.0F, 0.35F});
    assert(sliding.collided);
    assert(sliding.position.x > -0.30F);
    assert(sliding.position.z < 1.01F);

    // A platform-reduced portal that is too short for the camera must behave
    // as a closed door even though a directed portal record still exists.
    PreviewScene lowOpening = scene;
    for (PreviewPortal& portal : lowOpening.portals) {
        portal.upperLeft.y = 0.90F;
        portal.upperRight.y = 0.90F;
    }
    const PreviewMovementResult lowBlocked = MovePreviewCameraWithCollision(
        lowOpening,
        0U,
        {-0.25F, 1.0F, 0.0F},
        {0.75F, 0.0F, 0.0F});
    assert(lowBlocked.polygonID == 0U);
    assert(lowBlocked.collided);

    const auto interaction = FindPreviewDoorForInteraction(
        scene,
        0U,
        {-0.5F, 1.0F, 0.0F},
        {1.0F, 0.0F, 0.0F});
    assert(interaction.has_value());
    assert(*interaction == 0U);

    assert(ResolveLevelEnvironmentCollection(1, 4, true) == 4);
    assert(ResolveLevelEnvironmentCollection(11, 4, true) == 11);
    assert(NormalizeClassicCollection(21) == 4);

    return 0;
}
