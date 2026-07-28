// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#include "PreviewScene.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace pfhorge::preview {

struct PreviewCamera final {
    Vec3 position;
    float yawRadians = 0.0F;
    float pitchRadians = 0.0F;
    float verticalFieldOfViewRadians = 1.0471975512F;
    float nearPlane = 0.05F;
    float farPlane = 32768.0F;
    StableID containingPolygon = kInvalidID;
};

struct PickingResult final {
    SurfaceID surface;
    float depth = 1.0F;
};

class PreviewRenderer {
public:
    virtual ~PreviewRenderer() = default;

    PreviewRenderer(const PreviewRenderer&) = delete;
    PreviewRenderer& operator=(const PreviewRenderer&) = delete;
    PreviewRenderer(PreviewRenderer&&) = delete;
    PreviewRenderer& operator=(PreviewRenderer&&) = delete;

    virtual void resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void beginFrame(const PreviewCamera& camera) = 0;
    virtual void drawSurfaces(std::span<const PreviewSurface> surfaces) = 0;
    virtual void endFrame() = 0;

    [[nodiscard]] virtual std::optional<PickingResult> pick(
        std::uint32_t x,
        std::uint32_t y) = 0;

protected:
    PreviewRenderer() = default;
};

}  // namespace pfhorge::preview
