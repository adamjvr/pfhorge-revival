// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#ifdef __OBJC__

#import "LELevelData.h"
#import "LELine.h"
#import "LEMapPoint.h"
#import "LEMapObject.h"
#import "LEPolygon.h"
#import "LESide.h"
#import "PhMedia.h"
#import "PhLight.h"
#import "PhPlatform.h"

#include "PreviewScene.hpp"
#include "PreviewTexture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace pfhorge::preview {

struct PreviewSceneBuildOptions final {
    bool followLevelEnvironment = true;
    std::vector<float> platformExtensions;
};

namespace detail {

constexpr float kWorldUnitScale = 1.0F / 1024.0F;
constexpr float kTicksPerSecond = 60.0F;

[[nodiscard]] inline Vec3 pointAtHeight(
    LEMapPoint *point,
    short height) noexcept
{
    return Vec3{
        static_cast<float>(point.x) * kWorldUnitScale,
        static_cast<float>(height) * kWorldUnitScale,
        -static_cast<float>(point.y) * kWorldUnitScale,
    };
}

[[nodiscard]] inline Vec2 textureCoordinateForPoint(
    LEMapPoint *point,
    NSPoint origin) noexcept
{
    return Vec2{
        (static_cast<float>(point.x) -
         static_cast<float>(origin.x)) * kWorldUnitScale,
        (static_cast<float>(point.y) -
         static_cast<float>(origin.y)) * kWorldUnitScale,
    };
}

inline void hashValue(std::uint64_t& hash, std::uint64_t value) noexcept
{
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
        hash ^= (value >> shift) & 0xFFU;
        hash *= kPrime;
    }
}

inline void recordTextureReference(
    PreviewTextureAudit& audit,
    const TextureDescriptor& descriptor,
    bool remapped) noexcept
{
    ++audit.totalReferences;
    if (descriptor.collection < 0 || descriptor.bitmap < 0) {
        ++audit.emptyReferences;
        return;
    }
    if (!IsClassicTextureCollection(descriptor.collection)) {
        ++audit.invalidReferences;
        return;
    }
    ++audit.validReferences;
    if (remapped) {
        ++audit.remappedReferences;
    }
}

[[nodiscard]] inline TextureDescriptor textureDescriptorForShape(
    shape_descriptor shape,
    short transferMode,
    short environmentCode,
    bool followLevelEnvironment,
    PreviewTextureAudit& audit) noexcept
{
    if (shape == NONE) {
        TextureDescriptor empty;
        recordTextureReference(audit, empty, false);
        return empty;
    }

    const std::uint16_t bits = static_cast<std::uint16_t>(shape);
    const std::int16_t rawCollection =
        static_cast<std::int16_t>((bits >> 8U) & 0x1FU);
    const std::int16_t rawBitmap =
        static_cast<std::int16_t>(bits & 0xFFU);
    const std::int16_t normalized =
        NormalizeClassicCollection(rawCollection);
    const std::int16_t resolved = ResolveLevelEnvironmentCollection(
        normalized,
        environmentCode,
        followLevelEnvironment);
    const bool remapped = resolved != normalized;

    TextureDescriptor descriptor{
        resolved,
        rawBitmap,
        static_cast<std::int16_t>(transferMode),
    };
    descriptor.rawCollection = rawCollection;
    descriptor.rawBitmap = rawBitmap;
    descriptor.source = TextureDescriptorSource::PackedShape;
    descriptor.environmentRemapped = remapped;

    if (remapped) {
        ++audit.structuralEnvironmentRemaps;
    }

    recordTextureReference(audit, descriptor, remapped);
    return descriptor;
}

[[nodiscard]] inline TextureDescriptor textureDescriptorForSideDefinition(
    const side_texture_definition& definition,
    short transferMode,
    short environmentCode,
    bool followLevelEnvironment,
    PreviewTextureAudit& audit) noexcept
{
    if (definition.texture != NONE) {
        return textureDescriptorForShape(
            definition.texture,
            transferMode,
            environmentCode,
            followLevelEnvironment,
            audit);
    }

    // Some older Pfhorge documents retained the editor-only split collection
    // and texture fields even when the packed descriptor was empty. Preserve
    // those maps rather than treating every wall as untextured.
    if (definition.textureCollection >= 0 &&
        definition.textureNumber >= 0) {
        const std::int16_t normalized = NormalizeClassicCollection(
            definition.textureCollection);
        const std::int16_t resolved = ResolveLevelEnvironmentCollection(
            normalized,
            environmentCode,
            followLevelEnvironment);
        const bool remapped = resolved != normalized;
        TextureDescriptor descriptor{
            resolved,
            definition.textureNumber,
            static_cast<std::int16_t>(transferMode),
        };
        descriptor.rawCollection = definition.textureCollection;
        descriptor.rawBitmap = definition.textureNumber;
        descriptor.source = TextureDescriptorSource::LegacySplitFields;
        descriptor.environmentRemapped = remapped;

        if (remapped) {
            ++audit.structuralEnvironmentRemaps;
        }

        recordTextureReference(
            audit,
            descriptor,
            remapped);
        return descriptor;
    }

    TextureDescriptor empty;
    recordTextureReference(audit, empty, false);
    return empty;
}

struct ClassicMediaTexture final {
    std::int16_t rawCollection = -1;
    std::int16_t bitmap = -1;
    std::int16_t transferMode = 0;

    [[nodiscard]] bool valid() const noexcept {
        return rawCollection >= 0 && bitmap >= 0;
    }
};

[[nodiscard]] inline ClassicMediaTexture classicMediaTextureForType(
    short mediaType) noexcept
{
    switch (mediaType) {
        case _media_water:
            return ClassicMediaTexture{17, 19, 0};
        case _media_lava:
            return ClassicMediaTexture{18, 12, 0};
        case _media_goo:
            return ClassicMediaTexture{21, 5, 0};
        case _media_sewage:
            return ClassicMediaTexture{19, 13, 0};
        case _media_jjaro:
            return ClassicMediaTexture{20, 13, 0};
        default:
            return ClassicMediaTexture{};
    }
}

[[nodiscard]] inline TextureDescriptor textureDescriptorForMedia(
    PhMedia *media,
    PreviewTextureAudit& audit) noexcept
{
    if (media == nil) {
        TextureDescriptor empty;
        recordTextureReference(audit, empty, false);
        return empty;
    }

    const std::uint16_t archivedBits =
        static_cast<std::uint16_t>(media.texture);
    const std::int16_t archivedCollection =
        media.texture == NONE
            ? static_cast<std::int16_t>(-1)
            : static_cast<std::int16_t>((archivedBits >> 8U) & 0x1FU);
    const std::int16_t archivedBitmap =
        media.texture == NONE
            ? static_cast<std::int16_t>(-1)
            : static_cast<std::int16_t>(archivedBits & 0xFFU);

    const ClassicMediaTexture definition =
        classicMediaTextureForType(media.type);

    if (!definition.valid()) {
        if (media.texture == NONE) {
            TextureDescriptor empty;
            recordTextureReference(audit, empty, false);
            return empty;
        }

        TextureDescriptor fallback{
            NormalizeClassicCollection(archivedCollection),
            archivedBitmap,
            static_cast<std::int16_t>(media.transferMode),
        };
        fallback.rawCollection = archivedCollection;
        fallback.rawBitmap = archivedBitmap;
        fallback.source = TextureDescriptorSource::PackedShape;
        recordTextureReference(audit, fallback, false);
        return fallback;
    }

    TextureDescriptor descriptor{
        NormalizeClassicCollection(definition.rawCollection),
        definition.bitmap,
        definition.transferMode,
    };
    descriptor.rawCollection = archivedCollection;
    descriptor.rawBitmap = archivedBitmap;
    descriptor.source = TextureDescriptorSource::MediaDefinition;

    if (archivedCollection != definition.rawCollection ||
        archivedBitmap != definition.bitmap ||
        media.transferMode != definition.transferMode) {
        ++audit.mediaDefinitionTextureRepairs;
    }

    recordTextureReference(audit, descriptor, false);
    return descriptor;
}

[[nodiscard]] inline int nextInitialLightState(
    int state,
    bool stateless) noexcept
{
    switch (state) {
        case PhLightStateBecomingActive:
            return PhLightStatePrimaryActive;
        case PhLightStatePrimaryActive:
            return PhLightStateSecondaryActive;
        case PhLightStateSecondaryActive:
            return stateless
                ? PhLightStateBecomingInactive
                : PhLightStatePrimaryActive;
        case PhLightStateBecomingInactive:
            return PhLightStatePrimaryInactive;
        case PhLightStatePrimaryInactive:
            return PhLightStateSecondaryInactive;
        case PhLightStateSecondaryInactive:
            return stateless
                ? PhLightStateBecomingActive
                : PhLightStatePrimaryInactive;
        default:
            return state;
    }
}

[[nodiscard]] inline std::int32_t evaluateInitialLightFunction(
    PhLightFunction function,
    std::int32_t initialIntensity,
    std::int32_t finalIntensity,
    int phase,
    int period) noexcept
{
    if (period <= 0) {
        return finalIntensity;
    }

    const double t = std::clamp(
        static_cast<double>(phase) /
            static_cast<double>(period),
        0.0,
        1.0);

    switch (function) {
        case PhLightFunctionConstant:
            return finalIntensity;

        case PhLightFunctionLinear:
            return static_cast<std::int32_t>(std::llround(
                static_cast<double>(initialIntensity) +
                static_cast<double>(finalIntensity - initialIntensity) * t));

        case PhLightFunctionSmooth: {
            constexpr double kPi = 3.14159265358979323846;
            const double eased =
                0.5 - (0.5 * std::cos(kPi * t));
            return static_cast<std::int32_t>(std::llround(
                static_cast<double>(initialIntensity) +
                static_cast<double>(finalIntensity - initialIntensity) *
                    eased));
        }

        case PhLightFunctionFlicker: {
            constexpr double kPi = 3.14159265358979323846;
            const double eased =
                0.5 - (0.5 * std::cos(kPi * t));
            return static_cast<std::int32_t>(std::llround(
                static_cast<double>(initialIntensity) +
                static_cast<double>(finalIntensity - initialIntensity) *
                    eased));
        }

        default:
            return finalIntensity;
    }
}

[[nodiscard]] inline std::int32_t initialLightIntensity(
    PhLight *light) noexcept
{
    if (light == nil) {
        return 0;
    }

    const bool initiallyActive =
        [light getFlag:PhLightStaticFlagIsInitiallyActive];
    const bool stateless =
        [light getFlag:PhLightStaticFlagIsStateless];

    // Aleph One seeds a newly-created light with the final intensity of its
    // secondary active/inactive state, then enters the corresponding primary
    // state before applying the archived phase. Keep that seed unchanged while
    // rephasing, matching new_light() + rephase_light() without RNG deltas.
    const PhLightState seedState = initiallyActive
        ? PhLightStateSecondaryActive
        : PhLightStateSecondaryInactive;
    const std::int32_t seedIntensity =
        static_cast<std::int32_t>(
            [light intensityForState:seedState]);

    int state = initiallyActive
        ? PhLightStatePrimaryActive
        : PhLightStatePrimaryInactive;
    int phase = std::max<int>(0, light.phase);

    for (int guard = 0; guard < 64; ++guard) {
        const PhLightState lightState =
            static_cast<PhLightState>(state);
        const int period = std::max<int>(
            0,
            [light periodForState:lightState]);

        // Zero-period functions are skipped in the original light system.
        if (period <= 0) {
            const int nextState =
                nextInitialLightState(state, stateless);
            if (nextState == state) {
                return std::clamp<std::int32_t>(
                    seedIntensity,
                    0,
                    65536);
            }
            state = nextState;
            continue;
        }

        if (phase < period) {
            const std::int32_t finalIntensity =
                static_cast<std::int32_t>(
                    [light intensityForState:lightState]);
            return std::clamp<std::int32_t>(
                evaluateInitialLightFunction(
                    [light functionForState:lightState],
                    seedIntensity,
                    finalIntensity,
                    phase,
                    period),
                0,
                65536);
        }

        phase -= period;
        const int nextState =
            nextInitialLightState(state, stateless);
        if (nextState == state) {
            break;
        }
        state = nextState;
    }

    return std::clamp<std::int32_t>(
        seedIntensity,
        0,
        65536);
}

[[nodiscard]] inline short resolvedMediaHeight(
    PhMedia *media,
    PreviewTextureAudit& audit) noexcept
{
    if (media == nil) {
        return 0;
    }

    const int low = static_cast<int>(media.low);
    const int high = static_cast<int>(media.high);
    const int minimum = std::min(low, high);
    const int maximum = std::max(low, high);

    if (media.lightObject != nil) {
        const std::int32_t intensity =
            initialLightIntensity(media.lightObject);
        const std::int64_t delta =
            static_cast<std::int64_t>(high - low);
        const int derived = low + static_cast<int>(
            (delta * static_cast<std::int64_t>(intensity)) >> 16);
        ++audit.mediaHeightsDerivedFromLight;
        return static_cast<short>(
            std::clamp(derived, minimum, maximum));
    }

    // Aleph One's get_light_intensity() returns zero for an absent/invalid
    // light, so the runtime equation resolves to the low endpoint. The archived
    // `height` member is derived runtime state, not the source of truth.
    ++audit.mediaHeightFallbacks;
    return static_cast<short>(
        std::clamp(low, minimum, maximum));
}

struct ResolvedSide final {
    LESide *side = nil;
};

[[nodiscard]] inline ResolvedSide resolvedSideForPolygon(
    LEPolygon *polygon,
    NSUInteger edgeIndex,
    NSArray<LESide *> *allSides,
    PreviewTextureAudit& audit) noexcept
{
    ++audit.sideResolutionAttempts;
    if (polygon == nil) {
        ++audit.sideResolutionMisses;
        return ResolvedSide{};
    }

    const short edge = static_cast<short>(edgeIndex);
    LELine *line = [polygon lineObjectAtIndex:edge];
    LESide *direct = [polygon sideObjectAtIndex:edge];

    // Marathon's line record owns the clockwise and counterclockwise side
    // indexes. Prefer those relationships when the polygon owner is known.
    // This avoids a dangerous legacy accessor: LEPolygon-sideIndexesAtIndex:
    // returns zero when the cached side pointer is nil, which used to alias
    // every unresolved wall to allSides[0].
    if (line != nil && line.clockwisePolygonObject == polygon) {
        LESide *candidate = line.clockwisePolygonSideObject;
        if (candidate != nil) {
            ++audit.sidesResolvedClockwise;
            return ResolvedSide{candidate};
        }
    }
    if (line != nil && line.conterclockwisePolygonObject == polygon) {
        LESide *candidate = line.counterclockwisePolygonSideObject;
        if (candidate != nil) {
            ++audit.sidesResolvedCounterclockwise;
            return ResolvedSide{candidate};
        }
    }

    // A polygon-side pointer reconstructed directly from the polygon record is
    // still useful, but reject it when it explicitly belongs to another line.
    if (direct != nil) {
        if (line == nil ||
            direct.lineObject == nil ||
            direct.lineObject == line) {
            ++audit.sidesResolvedDirect;
            return ResolvedSide{direct};
        }
        ++audit.rejectedStalePolygonSides;
    }

    if (line == nil) {
        ++audit.sideResolutionMisses;
        return ResolvedSide{};
    }

    // Some old editor files have stale polygon-owner pointers on the line but
    // intact side back-references.
    LESide *clockwise = line.clockwisePolygonSideObject;
    if (clockwise != nil && clockwise.polygonObject == polygon) {
        ++audit.sidesResolvedBackReference;
        return ResolvedSide{clockwise};
    }
    LESide *counterclockwise = line.counterclockwisePolygonSideObject;
    if (counterclockwise != nil &&
        counterclockwise.polygonObject == polygon) {
        ++audit.sidesResolvedBackReference;
        return ResolvedSide{counterclockwise};
    }

    // Last-resort recovery for a map whose polygon and line caches are stale
    // while the side's own line/polygon back-references remain coherent.
    for (LESide *candidate in allSides) {
        if (candidate.lineObject == line &&
            candidate.polygonObject == polygon) {
            ++audit.sidesResolvedScan;
            return ResolvedSide{candidate};
        }
    }

    ++audit.sideResolutionMisses;
    return ResolvedSide{};
}

struct WallTextureSelection final {
    TextureDescriptor descriptor;
    std::int16_t lightIndex = -1;
    float horizontalOffset = 0.0F;
    float verticalOffset = 0.0F;
};

[[nodiscard]] inline SurfaceTextureLayer wallTextureLayerForSegment(
    LESide *side,
    std::uint16_t segmentIndex) noexcept
{
    return side != nil &&
           side.type == LESideSplit &&
           segmentIndex == 0U
        ? SurfaceTextureLayer::Secondary
        : SurfaceTextureLayer::Primary;
}

[[nodiscard]] inline bool sideTextureDefinitionHasReference(
    const side_texture_definition& definition) noexcept
{
    return definition.texture != NONE ||
           (definition.textureCollection >= 0 &&
            definition.textureNumber >= 0);
}

[[nodiscard]] inline WallTextureSelection wallTextureSelection(
    LESide *side,
    SurfaceTextureLayer textureLayer,
    short environmentCode,
    bool followLevelEnvironment,
    PreviewTextureAudit& audit) noexcept
{
    if (side == nil) {
        TextureDescriptor empty;
        recordTextureReference(audit, empty, false);
        return WallTextureSelection{};
    }

    side_texture_definition definition = side.primaryTextureStruct;
    short transferMode = side.primaryTransferMode;
    short lightIndex = [side primaryLightsourceIndex];

    if (textureLayer == SurfaceTextureLayer::Secondary) {
        definition = side.secondaryTextureStruct;
        transferMode = side.secondaryTransferMode;
        lightIndex = [side secondaryLightsourceIndex];
    } else if (textureLayer == SurfaceTextureLayer::Transparent) {
        definition = side.transparentTextureStruct;
        transferMode = side.transparentTransferMode;
        lightIndex = [side transparentLightsourceIndex];
    }

    return WallTextureSelection{
        textureDescriptorForSideDefinition(
            definition,
            transferMode,
            environmentCode,
            followLevelEnvironment,
            audit),
        static_cast<std::int16_t>(lightIndex),
        static_cast<float>(definition.x0) * kWorldUnitScale,
        static_cast<float>(definition.y0) * kWorldUnitScale,
    };
}

inline void appendTriangleFan(
    PreviewSurface& surface,
    bool reverseWinding)
{
    const std::uint32_t vertexCount =
        static_cast<std::uint32_t>(surface.vertices.size());

    if (vertexCount < 3U) {
        return;
    }

    for (std::uint32_t index = 1U;
         index + 1U < vertexCount;
         ++index) {
        surface.indices.push_back(0U);

        if (reverseWinding) {
            surface.indices.push_back(index + 1U);
            surface.indices.push_back(index);
        } else {
            surface.indices.push_back(index);
            surface.indices.push_back(index + 1U);
        }
    }
}

inline void appendWallSegment(
    PreviewScene& scene,
    StableID polygonID,
    std::uint16_t edgeIndex,
    std::uint16_t segmentIndex,
    LEMapPoint *first,
    LEMapPoint *second,
    short lowerHeight,
    short upperHeight,
    LESide *side,
    LELine *line,
    bool landscape,
    SurfaceTextureLayer textureLayer,
    bool translucent,
    short environmentCode,
    bool followLevelEnvironment)
{
    if (first == nil || second == nil || upperHeight <= lowerHeight) {
        return;
    }

    ++scene.textureAudit.wallSegments;
    if (side == nil) {
        ++scene.textureAudit.wallSegmentsWithoutSide;
    } else {
        ++scene.textureAudit.wallSegmentsWithSide;
    }

    const WallTextureSelection selection = wallTextureSelection(
        side,
        textureLayer,
        environmentCode,
        followLevelEnvironment,
        scene.textureAudit);
    if (selection.descriptor.collection >= 0 &&
        selection.descriptor.bitmap >= 0 &&
        IsClassicTextureCollection(selection.descriptor.collection)) {
        ++scene.textureAudit.wallSegmentsWithTexture;
    } else {
        ++scene.textureAudit.wallSegmentsWithoutTexture;
    }

    if (textureLayer == SurfaceTextureLayer::Transparent) {
        ++scene.textureAudit.transparentWallSegments;
    }

    const float deltaX =
        static_cast<float>(second.x - first.x) * kWorldUnitScale;
    const float deltaY =
        static_cast<float>(second.y - first.y) * kWorldUnitScale;
    const float segmentLength =
        std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
    const float segmentHeight =
        static_cast<float>(upperHeight - lowerHeight) * kWorldUnitScale;

    const float u0 = landscape ? 0.0F : selection.horizontalOffset;
    const float u1 = landscape
        ? 1.0F
        : selection.horizontalOffset + segmentLength;
    const float v0 = landscape
        ? 1.0F
        : selection.verticalOffset + segmentHeight;
    const float v1 = landscape ? 0.0F : selection.verticalOffset;

    PreviewSurface wall;
    wall.id = SurfaceID{
        landscape ? SurfaceKind::Landscape : SurfaceKind::Wall,
        polygonID,
        static_cast<std::uint16_t>(edgeIndex * 3U + segmentIndex),
    };
    wall.polygonID = polygonID;
    const short lineIndex = line != nil ? [line index] : -1;
    wall.lineID = lineIndex >= 0
        ? static_cast<StableID>(lineIndex)
        : kInvalidPreviewID;
    const short sideIndex = side != nil ? [side index] : -1;
    wall.sideID = sideIndex >= 0
        ? static_cast<StableID>(sideIndex)
        : kInvalidPreviewID;
    wall.edgeIndex = edgeIndex;
    wall.textureLayer = textureLayer;
    wall.texture = selection.descriptor;
    wall.lightIndex = selection.lightIndex;

    if (side != nil) {
        wall.sideType =
            static_cast<std::int16_t>(side.type);
        wall.sideFlags =
            static_cast<std::uint16_t>(side.flags);
        wall.isControlPanel =
            (side.flags & LESideIsControlPanel) != 0;
        wall.controlPanelType =
            wall.isControlPanel
                ? static_cast<std::int16_t>(side.controlPanelType)
                : static_cast<std::int16_t>(-1);

        if (wall.isControlPanel) {
            ++scene.textureAudit.controlPanelWallSegments;
        }
    }

    wall.translucent = translucent;
    wall.vertices = {
        PreviewVertex{
            pointAtHeight(first, lowerHeight),
            Vec2{u0, v0},
            1.0F,
        },
        PreviewVertex{
            pointAtHeight(second, lowerHeight),
            Vec2{u1, v0},
            1.0F,
        },
        PreviewVertex{
            pointAtHeight(second, upperHeight),
            Vec2{u1, v1},
            1.0F,
        },
        PreviewVertex{
            pointAtHeight(first, upperHeight),
            Vec2{u0, v1},
            1.0F,
        },
    };
    wall.indices = {
        0U, 1U, 2U,
        0U, 2U, 3U,
    };

    scene.surfaces.push_back(std::move(wall));
}

[[nodiscard]] inline bool lineIsTransparent(LELine *line) noexcept
{
    return line != nil && (line.flags & LELineTransparent) != 0;
}

[[nodiscard]] inline StableID polygonIDForObject(
    LEPolygon *candidate,
    NSArray<LEPolygon *> *polygons) noexcept
{
    if (candidate == nil || polygons == nil) {
        return kInvalidPreviewID;
    }

    const NSUInteger index =
        [polygons indexOfObjectIdenticalTo:candidate];
    return index == NSNotFound
        ? kInvalidPreviewID
        : static_cast<StableID>(index);
}

[[nodiscard]] inline StableID oppositePolygonIDForLineOwner(
    LEPolygon *polygon,
    NSUInteger edgeIndex,
    NSArray<LEPolygon *> *polygons) noexcept
{
    if (polygon == nil || polygons == nil) {
        return kInvalidPreviewID;
    }

    LELine *line = [polygon lineObjectAtIndex:
        static_cast<short>(edgeIndex)];
    if (line == nil) {
        return kInvalidPreviewID;
    }

    LEPolygon *candidate = nil;
    if (line.clockwisePolygonObject == polygon) {
        candidate = line.conterclockwisePolygonObject;
    } else if (line.conterclockwisePolygonObject == polygon) {
        candidate = line.clockwisePolygonObject;
    }

    if (candidate == nil || candidate == polygon) {
        return kInvalidPreviewID;
    }

    return polygonIDForObject(candidate, polygons);
}

/**
 * Resolves the polygon across one transparent line without using the legacy
 * LEPolygon-adjacentPolygonIndexesAtIndex: nil-to-zero accessor.
 *
 * Old Pfhorge documents can have an empty polygon adjacency cache while the
 * LELine record still owns correct clockwise/counterclockwise polygon links.
 * Treat line ownership as authoritative, then use a non-nil direct polygon
 * pointer only as a fallback.
 */
[[nodiscard]] inline StableID resolvedAdjacentPolygonID(
    LEPolygon *polygon,
    NSUInteger edgeIndex,
    NSArray<LEPolygon *> *polygons,
    PreviewTopologyAudit *audit) noexcept
{
    if (audit != nullptr) {
        ++audit->polygonEdges;
    }

    if (polygon == nil ||
        edgeIndex >= static_cast<NSUInteger>(
            std::max<short>(0, polygon.getTheVertexCount))) {
        return kInvalidPreviewID;
    }

    LELine *line = [polygon lineObjectAtIndex:
        static_cast<short>(edgeIndex)];

    if (!lineIsTransparent(line)) {
        return kInvalidPreviewID;
    }

    if (audit != nullptr) {
        ++audit->transparentEdges;
    }

    const auto acceptCandidate =
        [&](LEPolygon *candidate,
            std::uint32_t PreviewTopologyAudit::*counter) -> StableID {
            if (candidate == nil) {
                return kInvalidPreviewID;
            }

            if (candidate == polygon) {
                if (audit != nullptr) {
                    ++audit->rejectedSelfAdjacency;
                }
                return kInvalidPreviewID;
            }

            const StableID candidateID =
                polygonIDForObject(candidate, polygons);

            if (candidateID == kInvalidPreviewID) {
                return kInvalidPreviewID;
            }

            if (audit != nullptr) {
                ++(audit->*counter);
            }

            return candidateID;
        };

    if (line != nil && line.clockwisePolygonObject == polygon) {
        const StableID candidate =
            acceptCandidate(
                line.conterclockwisePolygonObject,
                &PreviewTopologyAudit::adjacencyResolvedClockwiseOwner);
        if (candidate != kInvalidPreviewID) {
            return candidate;
        }
    }

    if (line != nil && line.conterclockwisePolygonObject == polygon) {
        const StableID candidate =
            acceptCandidate(
                line.clockwisePolygonObject,
                &PreviewTopologyAudit::adjacencyResolvedCounterclockwiseOwner);
        if (candidate != kInvalidPreviewID) {
            return candidate;
        }
    }

    LEPolygon *direct =
        [polygon adjacentPolygonObjectAtIndex:
            static_cast<short>(edgeIndex)];

    const StableID directID =
        acceptCandidate(
            direct,
            &PreviewTopologyAudit::adjacencyResolvedDirect);

    if (directID != kInvalidPreviewID) {
        return directID;
    }

    if (audit != nullptr) {
        ++audit->adjacencyResolutionMisses;
    }

    return kInvalidPreviewID;
}

struct PlatformGeometry final {
    bool valid = false;
    short floorHeight = 0;
    short ceilingHeight = 0;
};

[[nodiscard]] inline Vec3 polygonCenter(
    LEPolygon *polygon,
    short floorHeight,
    short ceilingHeight) noexcept
{
    if (polygon == nil) {
        return Vec3{};
    }
    NSArray<LEMapPoint *> *vertices = polygon.vertexArray;
    if (vertices.count == 0U) {
        return Vec3{
            static_cast<float>(polygon.center.x) * kWorldUnitScale,
            (static_cast<float>(floorHeight) +
             static_cast<float>(ceilingHeight)) *
                0.5F * kWorldUnitScale,
            -static_cast<float>(polygon.center.y) * kWorldUnitScale,
        };
    }
    float x = 0.0F;
    float z = 0.0F;
    for (LEMapPoint *point in vertices) {
        x += static_cast<float>(point.x) * kWorldUnitScale;
        z -= static_cast<float>(point.y) * kWorldUnitScale;
    }
    const float divisor = static_cast<float>(vertices.count);
    return Vec3{
        x / divisor,
        (static_cast<float>(floorHeight) +
         static_cast<float>(ceilingHeight)) *
            0.5F * kWorldUnitScale,
        z / divisor,
    };
}

[[nodiscard]] inline float platformExtensionFor(
    const PreviewSceneBuildOptions& options,
    NSUInteger platformIndex,
    bool initiallyExtended) noexcept
{
    if (platformIndex < options.platformExtensions.size() &&
        std::isfinite(options.platformExtensions[platformIndex])) {
        return std::clamp(
            options.platformExtensions[platformIndex],
            0.0F,
            1.0F);
    }
    return initiallyExtended ? 1.0F : 0.0F;
}

inline void buildPlatformGeometry(
    LELevelData *levelData,
    NSArray<LEPolygon *> *polygons,
    const PreviewSceneBuildOptions& options,
    PreviewScene& scene,
    std::vector<short>& floorHeights,
    std::vector<short>& ceilingHeights)
{
    NSArray<PhPlatform *> *platforms = [levelData getPlatforms];
    scene.platforms.reserve(platforms.count);

    for (NSUInteger platformIndex = 0U;
         platformIndex < platforms.count;
         ++platformIndex) {
        PhPlatform *platform = platforms[platformIndex];
        const short polygonIndex = platform.polygonIndex;
        if (polygonIndex < 0 ||
            static_cast<NSUInteger>(polygonIndex) >= polygons.count) {
            continue;
        }

        LEPolygon *polygon = polygons[static_cast<NSUInteger>(polygonIndex)];
        const PhPlatformStaticFlags flags = platform.staticFlags;
        const bool fromFloor = (flags & PhPlatformComesFromFloor) != 0;
        const bool fromCeiling = (flags & PhPlatformComesFromCeiling) != 0;
        if (!fromFloor && !fromCeiling) {
            continue;
        }

        const short baseFloor = floorHeights[polygonIndex];
        const short baseCeiling = ceilingHeights[polygonIndex];
        short minimum = platform.minimumHeight;
        short maximum = platform.maximumHeight;
        if (minimum == NONE) {
            minimum = baseFloor;
        }
        if (maximum == NONE) {
            maximum = baseCeiling;
        }
        if (maximum < minimum) {
            std::swap(maximum, minimum);
        }
        if (maximum <= minimum) {
            minimum = baseFloor;
            maximum = baseCeiling;
        }

        const bool initiallyExtended =
            (flags & PhPlatformIsInitiallyExtended) != 0;
        const float extension = platformExtensionFor(
            options,
            platformIndex,
            initiallyExtended);

        short effectiveFloor = baseFloor;
        short effectiveCeiling = baseCeiling;
        if (fromFloor && !fromCeiling) {
            effectiveFloor = static_cast<short>(std::lround(
                static_cast<float>(minimum) +
                static_cast<float>(maximum - minimum) * extension));
        } else if (fromCeiling && !fromFloor) {
            effectiveCeiling = static_cast<short>(std::lround(
                static_cast<float>(maximum) -
                static_cast<float>(maximum - minimum) * extension));
        } else {
            const float midpoint =
                (static_cast<float>(minimum) +
                 static_cast<float>(maximum)) * 0.5F;
            effectiveFloor = static_cast<short>(std::lround(
                static_cast<float>(minimum) +
                (midpoint - static_cast<float>(minimum)) * extension));
            effectiveCeiling = static_cast<short>(std::lround(
                static_cast<float>(maximum) -
                (static_cast<float>(maximum) - midpoint) * extension));
        }

        effectiveFloor = std::clamp(
            effectiveFloor,
            minimum,
            maximum);
        effectiveCeiling = std::clamp(
            effectiveCeiling,
            minimum,
            maximum);
        if (effectiveCeiling < effectiveFloor) {
            const short midpoint = static_cast<short>(
                (static_cast<int>(effectiveFloor) +
                 static_cast<int>(effectiveCeiling)) / 2);
            effectiveFloor = midpoint;
            effectiveCeiling = midpoint;
        }

        floorHeights[polygonIndex] = effectiveFloor;
        ceilingHeights[polygonIndex] = effectiveCeiling;

        const float speed = std::max(
            0.05F,
            static_cast<float>(std::abs(platform.speed)) *
                kTicksPerSecond * kWorldUnitScale);
        PreviewPlatform previewPlatform;
        previewPlatform.id = static_cast<StableID>(platformIndex);
        previewPlatform.polygonID = static_cast<StableID>(polygonIndex);
        previewPlatform.type = static_cast<std::int16_t>(platform.type);
        previewPlatform.minimumHeight =
            static_cast<float>(minimum) * kWorldUnitScale;
        previewPlatform.maximumHeight =
            static_cast<float>(maximum) * kWorldUnitScale;
        previewPlatform.speedWorldUnitsPerSecond = speed;
        previewPlatform.initialExtension = initiallyExtended ? 1.0F : 0.0F;
        previewPlatform.center = polygonCenter(
            polygon,
            effectiveFloor,
            effectiveCeiling);
        previewPlatform.comesFromFloor = fromFloor;
        previewPlatform.comesFromCeiling = fromCeiling;
        previewPlatform.isDoor = (flags & PhPlatformIsDoor) != 0;
        previewPlatform.playerControllable =
            (flags & PhPlatformIsPlayerControllable) != 0;
        previewPlatform.locked = (flags & PhPlatformIsLocked) != 0;
        scene.platforms.push_back(previewPlatform);
    }
}

}  // namespace detail

/**
 * Computes a stable fingerprint of geometry, textures, environment metadata,
 * media, and platform definitions. Metal Visual Mode polls this inexpensive
 * digest so ordinary editor changes appear without saving, closing, or
 * reopening the map.
 */
[[nodiscard]] inline std::uint64_t ComputePreviewLevelFingerprint(
    LELevelData *levelData) noexcept
{
    if (levelData == nil) {
        return 0U;
    }

    std::uint64_t hash = 1469598103934665603ULL;
    detail::hashValue(hash, static_cast<std::uint16_t>(levelData.environmentCode));

    NSArray<LEMapPoint *> *points = [levelData points];
    detail::hashValue(hash, points.count);
    for (LEMapPoint *point in points) {
        detail::hashValue(hash, static_cast<std::uint32_t>(point.x));
        detail::hashValue(hash, static_cast<std::uint32_t>(point.y));
    }

    NSArray<LEPolygon *> *polygons = [levelData polygons];
    detail::hashValue(hash, polygons.count);
    for (LEPolygon *polygon in polygons) {
        detail::hashValue(hash, static_cast<std::uint16_t>(polygon.floorHeight));
        detail::hashValue(hash, static_cast<std::uint16_t>(polygon.ceilingHeight));
        detail::hashValue(hash, static_cast<std::uint16_t>(polygon.floorTexture));
        detail::hashValue(hash, static_cast<std::uint16_t>(polygon.ceilingTexture));
        detail::hashValue(hash, static_cast<std::uint16_t>(polygon.floorTransferMode));
        detail::hashValue(hash, static_cast<std::uint16_t>(polygon.ceilingTransferMode));
        detail::hashValue(hash, static_cast<std::uint32_t>(polygon.floorOrigin.x));
        detail::hashValue(hash, static_cast<std::uint32_t>(polygon.floorOrigin.y));
        detail::hashValue(hash, static_cast<std::uint32_t>(polygon.ceilingOrigin.x));
        detail::hashValue(hash, static_cast<std::uint32_t>(polygon.ceilingOrigin.y));
        const short vertexCount = polygon.getTheVertexCount;
        detail::hashValue(hash, static_cast<std::uint16_t>(vertexCount));
        for (short index = 0; index < vertexCount; ++index) {
            detail::hashValue(
                hash,
                static_cast<std::uint16_t>(
                    [polygon vertexIndexesAtIndex:index]));
            detail::hashValue(
                hash,
                static_cast<std::uint16_t>(
                    [polygon lineIndexesAtIndex:index]));
            LESide *polygonSide = [polygon sideObjectAtIndex:index];
            const short polygonSideIndex =
                polygonSide != nil ? [polygonSide index] : -1;
            detail::hashValue(
                hash,
                static_cast<std::uint16_t>(polygonSideIndex));
            const StableID adjacentPolygonID =
                detail::resolvedAdjacentPolygonID(
                    polygon,
                    static_cast<NSUInteger>(index),
                    polygons,
                    nullptr);
            detail::hashValue(
                hash,
                static_cast<std::uint64_t>(adjacentPolygonID));
        }
        PhMedia *media = polygon.mediaObject;
        if (media != nil) {
            detail::hashValue(hash, static_cast<std::uint16_t>(media.type));
            detail::hashValue(hash, static_cast<std::uint16_t>(media.low));
            detail::hashValue(hash, static_cast<std::uint16_t>(media.high));
            detail::hashValue(hash, static_cast<std::uint16_t>(media.height));
            detail::hashValue(hash, static_cast<std::uint16_t>(media.texture));
            detail::hashValue(hash, static_cast<std::uint16_t>(media.transferMode));

            PhLight *mediaLight = media.lightObject;
            if (mediaLight != nil) {
                detail::hashValue(
                    hash,
                    static_cast<std::uint16_t>(mediaLight.flags));
                detail::hashValue(
                    hash,
                    static_cast<std::uint16_t>(mediaLight.phase));
                for (int state = 0;
                     state < PhLightStateTotalCount;
                     ++state) {
                    const PhLightState lightState =
                        static_cast<PhLightState>(state);
                    detail::hashValue(
                        hash,
                        static_cast<std::uint16_t>(
                            [mediaLight functionForState:lightState]));
                    detail::hashValue(
                        hash,
                        static_cast<std::uint16_t>(
                            [mediaLight periodForState:lightState]));
                    detail::hashValue(
                        hash,
                        static_cast<std::uint32_t>(
                            [mediaLight intensityForState:lightState]));
                }
            }
        }
    }

    NSArray<LEMapObject *> *objects = [levelData theMapObjects];
    detail::hashValue(hash, objects.count);
    for (LEMapObject *object in objects) {
        detail::hashValue(hash, static_cast<std::uint16_t>(object.type));
        detail::hashValue(hash, static_cast<std::uint16_t>(object.x));
        detail::hashValue(hash, static_cast<std::uint16_t>(object.y));
        detail::hashValue(hash, static_cast<std::uint16_t>(object.z));
        detail::hashValue(hash, static_cast<std::uint16_t>(object.facing));
        detail::hashValue(hash, static_cast<std::uint16_t>(object.polygonIndex));
    }

    NSArray<LELine *> *lines = [levelData lines];
    detail::hashValue(hash, lines.count);
    for (LELine *line in lines) {
        detail::hashValue(hash, static_cast<std::uint16_t>(line.flags));
        detail::hashValue(hash, static_cast<std::uint16_t>(line.pointIndex1));
        detail::hashValue(hash, static_cast<std::uint16_t>(line.pointIndex2));
    }

    NSArray<LESide *> *sides = [levelData getSides];
    detail::hashValue(hash, sides.count);
    for (LESide *side in sides) {
        detail::hashValue(hash, static_cast<std::uint16_t>(side.type));
        detail::hashValue(hash, static_cast<std::uint16_t>(side.flags));
        const side_texture_definition definitions[] = {
            side.primaryTextureStruct,
            side.secondaryTextureStruct,
            side.transparentTextureStruct,
        };
        for (const side_texture_definition& definition : definitions) {
            detail::hashValue(hash, static_cast<std::uint16_t>(definition.texture));
            detail::hashValue(hash, static_cast<std::uint16_t>(definition.x0));
            detail::hashValue(hash, static_cast<std::uint16_t>(definition.y0));
            detail::hashValue(hash, static_cast<std::uint16_t>(definition.textureCollection));
            detail::hashValue(hash, static_cast<std::uint16_t>(definition.textureNumber));
        }
        detail::hashValue(hash, static_cast<std::uint16_t>(side.primaryTransferMode));
        detail::hashValue(hash, static_cast<std::uint16_t>(side.secondaryTransferMode));
        detail::hashValue(hash, static_cast<std::uint16_t>(side.transparentTransferMode));
    }

    NSArray<PhPlatform *> *platforms = [levelData getPlatforms];
    detail::hashValue(hash, platforms.count);
    for (PhPlatform *platform in platforms) {
        detail::hashValue(hash, static_cast<std::uint16_t>(platform.type));
        detail::hashValue(hash, static_cast<std::uint16_t>(platform.speed));
        detail::hashValue(hash, static_cast<std::uint16_t>(platform.delay));
        detail::hashValue(hash, static_cast<std::uint16_t>(platform.minimumHeight));
        detail::hashValue(hash, static_cast<std::uint16_t>(platform.maximumHeight));
        detail::hashValue(hash, static_cast<std::uint32_t>(platform.staticFlags));
        detail::hashValue(hash, static_cast<std::uint16_t>(platform.polygonIndex));
        detail::hashValue(hash, static_cast<std::uint16_t>(platform.tag));
    }

    return hash;
}

/**
 * Builds an immutable renderer snapshot from the live Objective-C map model.
 *
 * VM-4A resolves both direct polygon side pointers and line-owned fallback
 * sides, audits every classic texture reference, supports non-destructive
 * level-environment remapping, and evaluates temporary platform/door states.
 */
[[nodiscard]] inline PreviewScene BuildPreviewScene(
    LELevelData *levelData,
    const PreviewSceneBuildOptions& options)
{
    PreviewScene scene;
    scene.revision = 6U;

    if (levelData == nil) {
        return scene;
    }

    scene.environmentCode = levelData.environmentCode;

    NSArray<LEMapPoint *> *points = [levelData points];
    scene.endpoints.reserve(points.count);
    for (LEMapPoint *point in points) {
        scene.endpoints.push_back(Vec3{
            static_cast<float>(point.x) * detail::kWorldUnitScale,
            0.0F,
            -static_cast<float>(point.y) * detail::kWorldUnitScale,
        });
    }

    NSArray<LEPolygon *> *polygons = [levelData polygons];
    NSArray<LESide *> *allSides = [levelData getSides];
    std::vector<short> floorHeights(polygons.count, 0);
    std::vector<short> ceilingHeights(polygons.count, 0);
    for (NSUInteger index = 0U; index < polygons.count; ++index) {
        floorHeights[index] = polygons[index].floorHeight;
        ceilingHeights[index] = polygons[index].ceilingHeight;
    }
    detail::buildPlatformGeometry(
        levelData,
        polygons,
        options,
        scene,
        floorHeights,
        ceilingHeights);

    // Keep vector indexes identical to Marathon polygon indexes. Earlier
    // snapshots skipped malformed polygons, which could silently invalidate
    // portal, platform, and collision IDs after the first skipped polygon.
    scene.polygons.resize(polygons.count);

    for (NSUInteger polygonIndex = 0U;
         polygonIndex < polygons.count;
         ++polygonIndex) {
        LEPolygon *polygon = polygons[polygonIndex];
        NSArray<LEMapPoint *> *vertices = polygon.vertexArray;

        const NSUInteger requestedCount = static_cast<NSUInteger>(
            std::max<short>(0, polygon.getTheVertexCount));
        const NSUInteger vertexCount =
            std::min(vertices.count, requestedCount);
        const StableID stablePolygonID =
            static_cast<StableID>(polygonIndex);

        PreviewPolygon previewPolygon;
        previewPolygon.id = stablePolygonID;
        previewPolygon.floorHeight =
            static_cast<float>(floorHeights[polygonIndex]) *
            detail::kWorldUnitScale;
        previewPolygon.ceilingHeight =
            static_cast<float>(ceilingHeights[polygonIndex]) *
            detail::kWorldUnitScale;
        previewPolygon.endpointIDs.reserve(vertexCount);
        previewPolygon.adjacentPolygonIDs.reserve(vertexCount);

        for (NSUInteger vertexIndex = 0U;
             vertexIndex < vertexCount;
             ++vertexIndex) {
            LEMapPoint *point = vertices[vertexIndex];
            const NSUInteger endpointIndex =
                [points indexOfObjectIdenticalTo:point];
            previewPolygon.endpointIDs.push_back(
                endpointIndex == NSNotFound
                    ? kInvalidPreviewID
                    : static_cast<StableID>(endpointIndex));
            previewPolygon.adjacentPolygonIDs.push_back(
                detail::resolvedAdjacentPolygonID(
                    polygon,
                    vertexIndex,
                    polygons,
                    &scene.topologyAudit));
        }
        scene.polygons[polygonIndex] = previewPolygon;

        if (vertexCount < 3U) {
            continue;
        }

        PreviewSurface floor;
        floor.id = SurfaceID{SurfaceKind::Floor, stablePolygonID, 0U};
        floor.polygonID = stablePolygonID;
        floor.textureLayer = SurfaceTextureLayer::Floor;
        floor.texture = detail::textureDescriptorForShape(
            polygon.floorTexture,
            polygon.floorTransferMode,
            scene.environmentCode,
            options.followLevelEnvironment,
            scene.textureAudit);
        floor.lightIndex = polygon.floorLightsourceIndex;
        floor.vertices.reserve(vertexCount);

        PreviewSurface ceiling;
        ceiling.id = SurfaceID{SurfaceKind::Ceiling, stablePolygonID, 0U};
        ceiling.polygonID = stablePolygonID;
        ceiling.textureLayer = SurfaceTextureLayer::Ceiling;
        ceiling.texture = detail::textureDescriptorForShape(
            polygon.ceilingTexture,
            polygon.ceilingTransferMode,
            scene.environmentCode,
            options.followLevelEnvironment,
            scene.textureAudit);
        ceiling.lightIndex = polygon.ceilingLightsourceIndex;
        ceiling.vertices.reserve(vertexCount);

        for (NSUInteger vertexIndex = 0U;
             vertexIndex < vertexCount;
             ++vertexIndex) {
            LEMapPoint *point = vertices[vertexIndex];
            floor.vertices.push_back(PreviewVertex{
                detail::pointAtHeight(point, floorHeights[polygonIndex]),
                detail::textureCoordinateForPoint(point, polygon.floorOrigin),
                1.0F,
            });
            ceiling.vertices.push_back(PreviewVertex{
                detail::pointAtHeight(point, ceilingHeights[polygonIndex]),
                detail::textureCoordinateForPoint(point, polygon.ceilingOrigin),
                1.0F,
            });
        }
        detail::appendTriangleFan(floor, false);
        detail::appendTriangleFan(ceiling, true);
        scene.surfaces.push_back(std::move(floor));
        scene.surfaces.push_back(std::move(ceiling));

        PhMedia *media = polygon.mediaObject;
        const short mediaHeight = media != nil
            ? detail::resolvedMediaHeight(
                media,
                scene.textureAudit)
            : static_cast<short>(0);
        if (media != nil &&
            mediaHeight > floorHeights[polygonIndex] &&
            mediaHeight < ceilingHeights[polygonIndex]) {
            ++scene.textureAudit.mediaSurfaces;

            PreviewSurface mediaSurface;
            mediaSurface.id = SurfaceID{
                SurfaceKind::Media,
                stablePolygonID,
                0U,
            };
            mediaSurface.polygonID = stablePolygonID;
            mediaSurface.textureLayer = SurfaceTextureLayer::Media;
            mediaSurface.translucent = true;
            mediaSurface.mediaType =
                static_cast<std::int16_t>(media.type);
            mediaSurface.texture =
                detail::textureDescriptorForMedia(
                    media,
                    scene.textureAudit);
            mediaSurface.lightIndex = polygon.mediaLightsourceIndex;
            mediaSurface.vertices.reserve(vertexCount);
            for (NSUInteger vertexIndex = 0U;
                 vertexIndex < vertexCount;
                 ++vertexIndex) {
                LEMapPoint *point = vertices[vertexIndex];
                mediaSurface.vertices.push_back(PreviewVertex{
                    detail::pointAtHeight(point, mediaHeight),
                    detail::textureCoordinateForPoint(point, media.origin),
                    1.0F,
                });
            }
            detail::appendTriangleFan(mediaSurface, false);
            scene.surfaces.push_back(std::move(mediaSurface));
        }

        for (NSUInteger edgeIndex = 0U;
             edgeIndex < vertexCount;
             ++edgeIndex) {
            const NSUInteger nextIndex = (edgeIndex + 1U) % vertexCount;
            LEMapPoint *first = vertices[edgeIndex];
            LEMapPoint *second = vertices[nextIndex];
            LELine *line = [polygon lineObjectAtIndex:
                static_cast<short>(edgeIndex)];
            const detail::ResolvedSide sideResolution =
                detail::resolvedSideForPolygon(
                    polygon,
                    edgeIndex,
                    allSides,
                    scene.textureAudit);
            LESide *side = sideResolution.side;
            if (side != nil && side.type == LESideComposite) {
                ++scene.textureAudit.compositeSides;
            }
            const bool landscape =
                line != nil && (line.flags & LELineLandscape) != 0;
            const StableID adjacentID =
                previewPolygon.adjacentPolygonIDs[edgeIndex];

            // A side's transparent texture is an independent overlay pass.
            // It is not restricted to traversable portal lines.
            if (side != nil &&
                detail::sideTextureDefinitionHasReference(
                    side.transparentTextureStruct)) {
                ++scene.textureAudit.transparentSideReferences;

                short transparentBottom =
                    floorHeights[polygonIndex];
                short transparentTop =
                    ceilingHeights[polygonIndex];

                const StableID oppositeID =
                    detail::oppositePolygonIDForLineOwner(
                        polygon,
                        edgeIndex,
                        polygons);
                if (oppositeID != kInvalidPreviewID &&
                    oppositeID < polygons.count) {
                    transparentBottom = std::max(
                        transparentBottom,
                        floorHeights[oppositeID]);
                    transparentTop = std::min(
                        transparentTop,
                        ceilingHeights[oppositeID]);
                }

                if (transparentTop > transparentBottom) {
                    const bool transparentLandscape =
                        side.transparentTransferMode == 9 ||
                        side.transparentTransferMode == 21;

                    if (!detail::lineIsTransparent(line)) {
                        ++scene.textureAudit
                            .transparentOverlaysOnSolidLines;
                    }

                    detail::appendWallSegment(
                        scene,
                        stablePolygonID,
                        static_cast<std::uint16_t>(edgeIndex),
                        1U,
                        first,
                        second,
                        transparentBottom,
                        transparentTop,
                        side,
                        line,
                        transparentLandscape,
                        SurfaceTextureLayer::Transparent,
                        true,
                        scene.environmentCode,
                        options.followLevelEnvironment);
                }
            }

            if (adjacentID == kInvalidPreviewID ||
                adjacentID >= polygons.count) {
                detail::appendWallSegment(
                    scene,
                    stablePolygonID,
                    static_cast<std::uint16_t>(edgeIndex),
                    0U,
                    first,
                    second,
                    floorHeights[polygonIndex],
                    ceilingHeights[polygonIndex],
                    side,
                    line,
                    landscape,
                    detail::wallTextureLayerForSegment(side, 0U),
                    false,
                    scene.environmentCode,
                    options.followLevelEnvironment);
                continue;
            }

            const short openingBottom = std::max(
                floorHeights[polygonIndex],
                floorHeights[adjacentID]);
            const short openingTop = std::min(
                ceilingHeights[polygonIndex],
                ceilingHeights[adjacentID]);

            if (openingTop <= openingBottom) {
                detail::appendWallSegment(
                    scene,
                    stablePolygonID,
                    static_cast<std::uint16_t>(edgeIndex),
                    0U,
                    first,
                    second,
                    floorHeights[polygonIndex],
                    ceilingHeights[polygonIndex],
                    side,
                    line,
                    landscape,
                    detail::wallTextureLayerForSegment(side, 0U),
                    false,
                    scene.environmentCode,
                    options.followLevelEnvironment);
                continue;
            }

            scene.portals.push_back(PreviewPortal{
                stablePolygonID,
                adjacentID,
                static_cast<std::uint16_t>(edgeIndex),
                detail::pointAtHeight(first, openingBottom),
                detail::pointAtHeight(second, openingBottom),
                detail::pointAtHeight(second, openingTop),
                detail::pointAtHeight(first, openingTop),
            });

            detail::appendWallSegment(
                scene,
                stablePolygonID,
                static_cast<std::uint16_t>(edgeIndex),
                0U,
                first,
                second,
                floorHeights[polygonIndex],
                openingBottom,
                side,
                line,
                landscape,
                detail::wallTextureLayerForSegment(side, 0U),
                false,
                scene.environmentCode,
                options.followLevelEnvironment);
            detail::appendWallSegment(
                scene,
                stablePolygonID,
                static_cast<std::uint16_t>(edgeIndex),
                2U,
                first,
                second,
                openingTop,
                ceilingHeights[polygonIndex],
                side,
                line,
                landscape,
                detail::wallTextureLayerForSegment(side, 2U),
                false,
                scene.environmentCode,
                options.followLevelEnvironment);
        }
    }

    NSArray<LEMapObject *> *mapObjects = [levelData theMapObjects];
    scene.playerStarts.reserve(mapObjects.count);
    for (NSUInteger objectIndex = 0U;
         objectIndex < mapObjects.count;
         ++objectIndex) {
        LEMapObject *object = mapObjects[objectIndex];
        if (object.type != _saved_player) {
            continue;
        }
        const short polygonIndex = object.polygonIndex;
        if (polygonIndex < 0 ||
            static_cast<NSUInteger>(polygonIndex) >= polygons.count) {
            continue;
        }
        const float floorHeight =
            static_cast<float>(floorHeights[polygonIndex]) *
            detail::kWorldUnitScale;
        const float ceilingHeight =
            static_cast<float>(ceilingHeights[polygonIndex]) *
            detail::kWorldUnitScale;
        const float clearance = ceilingHeight - floorHeight;
        if (clearance <= 0.20F) {
            continue;
        }

        const float requestedEyeHeight =
            floorHeight +
            static_cast<float>(object.z) * detail::kWorldUnitScale +
            0.50F;
        const float eyeHeight = std::clamp(
            requestedEyeHeight,
            floorHeight + 0.05F,
            ceilingHeight - 0.05F);

        constexpr float kTwoPi = 6.28318530717958647692F;
        constexpr float kMarathonAngleUnits = 512.0F;
        const float marathonAngle =
            static_cast<float>(object.facing) *
            kTwoPi / kMarathonAngleUnits;

        PreviewPlayerStart start;
        start.objectID = static_cast<StableID>(objectIndex);
        start.polygonID = static_cast<StableID>(polygonIndex);
        start.position = Vec3{
            static_cast<float>(object.x) * detail::kWorldUnitScale,
            eyeHeight,
            -static_cast<float>(object.y) * detail::kWorldUnitScale,
        };
        start.yawRadians =
            1.57079632679489661923F - marathonAngle;
        scene.playerStarts.push_back(start);
    }

    return scene;
}

[[nodiscard]] inline PreviewScene BuildPreviewScene(
    LELevelData *levelData)
{
    return BuildPreviewScene(levelData, PreviewSceneBuildOptions{});
}

}  // namespace pfhorge::preview

#endif  // __OBJC__
