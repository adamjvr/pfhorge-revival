// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#include "../Core/PreviewTexture.hpp"

#include <cassert>
#include <cstdint>

int main()
{
    using namespace pfhorge::preview;

    static_assert(NormalizeClassicCollection(17) == 0);
    static_assert(NormalizeClassicCollection(21) == 4);
    static_assert(NormalizeClassicCollection(27) == 10);
    static_assert(NormalizeClassicCollection(30) == 13);
    static_assert(NormalizeClassicCollection(3) == 3);
    static_assert(IsClassicTextureCollection(0));
    static_assert(IsClassicTextureCollection(13));
    static_assert(!IsClassicTextureCollection(9));

    PreviewSurface surface;
    surface.texture.collection = 18;
    surface.texture.bitmap = 42;
    const PreviewTextureKey key = ClassicTextureKeyFor(surface);

    assert(key.valid());
    assert(key.collection == 1);
    assert(key.bitmap == 42);
    assert(key.packed() ==
        ((static_cast<std::uint32_t>(1) << 16U) | 42U));
    assert(ClassicSurfaceOpacity(SurfaceKind::Wall) == 1.0F);
    assert(ClassicSurfaceOpacity(SurfaceKind::Media) == 0.65F);

    surface.texture.bitmap = -1;
    assert(!ClassicTextureKeyFor(surface).valid());

    return 0;
}
