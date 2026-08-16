// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
#pragma once
#include "PfhorgeCanonicalFoundation.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pfhorge::format {
using Scalar = std::int64_t;
struct Point2 { Scalar x{}; Scalar y{}; };
struct TextureRef { std::int64_t collection{}; std::int64_t bitmap{}; std::int64_t clut{}; };
struct SurfaceLayer { std::optional<TextureRef> texture; Point2 offset{}; Scalar transferMode{}; std::optional<StableId> light; };
struct Point { StableId id; Scalar x{}; Scalar y{}; };
struct Line { StableId id; StableId startPoint; StableId endPoint; std::uint64_t flags{}; };
enum class EdgeDirection : std::uint8_t { Forward, Reverse };
struct PolygonEdge { StableId line; std::optional<StableId> side; EdgeDirection direction{EdgeDirection::Forward}; };
struct SemanticTarget { std::string kind{"none"}; std::optional<Scalar> value; std::optional<StableId> target; };
struct Plane { Scalar height{}; std::optional<TextureRef> texture; Scalar transferMode{}; Point2 origin{}; std::optional<StableId> light; };
struct Polygon { StableId id; Scalar type{}; std::uint64_t flags{}; SemanticTarget permutation; std::vector<PolygonEdge> edges; Plane floor; Plane ceiling; std::optional<StableId> media; std::optional<StableId> ambientSound; std::optional<StableId> randomSound; };
struct ControlPanel { Scalar type{}; SemanticTarget target; };
enum class SideType : std::uint8_t { Full, High, Low, Composite, Split };
struct Side { StableId id; StableId line; StableId polygon; SideType type{SideType::Full}; std::uint64_t flags{}; SurfaceLayer primary; std::optional<SurfaceLayer> secondary; std::optional<SurfaceLayer> transparent; std::optional<ControlPanel> controlPanel; Scalar ambientDelta{}; };
struct LightFunction { Scalar function{}; Scalar period{}; Scalar deltaPeriod{}; Scalar intensity{}; Scalar deltaIntensity{}; };
struct Light { StableId id; Scalar type{}; std::uint64_t flags{}; Scalar phase{}; std::array<LightFunction,6> states{}; std::optional<StableId> tag; };
enum class MediaAppearanceMode : std::uint8_t { TypeDefault, Explicit };
struct MediaAppearance { MediaAppearanceMode mode{MediaAppearanceMode::TypeDefault}; std::optional<TextureRef> texture; Scalar transferMode{}; };
struct Media { StableId id; Scalar type{}; std::uint64_t flags{}; std::optional<StableId> light; Scalar currentDirection{}; Scalar currentMagnitude{}; Scalar low{}; Scalar high{}; Point2 origin{}; Scalar minimumLightIntensity{}; MediaAppearance appearance; };
struct Platform { StableId id; Scalar type{}; Scalar speed{}; Scalar delay{}; Scalar maximumHeight{}; Scalar minimumHeight{}; std::uint64_t flags{}; StableId polygon; std::optional<StableId> tag; };
struct MapObject { StableId id; Scalar type{}; Scalar kindIndex{}; Scalar facing{}; std::optional<StableId> polygon; Scalar x{}; Scalar y{}; Scalar z{}; std::uint64_t flags{}; };
struct ItemPlacement { StableId id; std::uint64_t slot{}; std::uint64_t flags{}; Scalar initialCount{}; Scalar minimumCount{}; Scalar maximumCount{}; Scalar randomCount{}; std::uint16_t randomChance{}; };
struct AmbientSound { StableId id; std::uint64_t flags{}; Scalar soundIndex{}; Scalar volume{}; };
struct RandomSound { StableId id; std::uint64_t flags{}; Scalar soundIndex{}; Scalar volume{}; Scalar deltaVolume{}; Scalar period{}; Scalar deltaPeriod{}; Scalar direction{}; Scalar deltaDirection{}; Scalar pitch{}; Scalar deltaPitch{}; Scalar phase{}; };
struct Tag { StableId id; Scalar number{}; };
struct Annotation { StableId id; Scalar type{}; Point2 location{}; std::optional<StableId> polygon; std::string text; };
struct TerminalStyleRun { std::uint64_t start{}; std::uint64_t length{}; std::uint16_t face{}; std::uint8_t colorIndex{}; };
struct TerminalSection { StableId id; Scalar type{}; std::uint64_t flags{}; SemanticTarget target; std::string text; std::vector<TerminalStyleRun> styles; };
struct Terminal { StableId id; Scalar linesPerPage{}; std::vector<TerminalSection> sections; };
struct EditorLineOverride { StableId line; bool enabled{}; bool solid{}; bool transparent{}; bool landscape{}; bool noSides{}; };
struct EditorLayer { StableId id; std::string name; std::vector<StableId> members; };
struct LevelMetadata { std::optional<Scalar> classicEnvironmentCode; std::optional<Scalar> physicsModel; std::optional<Scalar> songIndex; std::optional<std::uint64_t> missionFlags; std::optional<std::uint64_t> environmentFlags; std::optional<std::uint64_t> entryPointFlags; };
struct CanonicalLevel {
    StableId id; std::string name; LevelMetadata metadata;
    std::vector<Point> points; std::vector<Line> lines; std::vector<Polygon> polygons; std::vector<Side> sides;
    std::vector<Light> lights; std::vector<Media> media; std::vector<Platform> platforms; std::vector<MapObject> objects;
    std::vector<ItemPlacement> itemPlacements; std::vector<AmbientSound> ambientSounds; std::vector<RandomSound> randomSounds;
    std::vector<Tag> tags; std::vector<Annotation> annotations; std::vector<Terminal> terminals; std::vector<EditorLayer> layers;
    std::vector<EditorLineOverride> lineOverrides; Provenance provenance;
};
} // namespace pfhorge::format
