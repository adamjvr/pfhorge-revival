// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#include "../Core/PreviewRenderer.hpp"

#include <cassert>

int main() {
    using namespace pfhorge::preview;

    PreviewScene scene;
    assert(scene.empty());

    PreviewPolygon polygon;
    polygon.id = 1;
    polygon.floorHeight = 0.0F;
    polygon.ceilingHeight = 2.0F;
    scene.polygons.push_back(polygon);

    assert(!scene.empty());
    assert(scene.polygons.front().id == 1);
    return 0;
}
