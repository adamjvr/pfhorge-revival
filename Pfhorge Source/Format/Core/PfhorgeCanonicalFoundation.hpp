// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pfhorge::format {

using StableIdBytes = std::array<std::uint8_t, 16>;

struct StableId {
    StableIdBytes bytes{};
    [[nodiscard]] constexpr bool isNil() const noexcept {
        for (auto b : bytes) if (b != 0U) return false;
        return true;
    }
    friend constexpr bool operator==(const StableId&, const StableId&) = default;
};

enum class DocumentKind : std::uint8_t { Level, Scenario, Resource };
enum class FieldRole : std::uint8_t {
    AuthoritativeGame, AuthoritativeEditor, SourceProvenance, Derived,
    Cache, RuntimeOnly, OpaqueSource, DeprecatedCompatibility, NeedsReview
};
enum class RoundTripPolicy : std::uint8_t {
    Semantic, PreserveEditor, PreserveRawIfPresent, Recompute, Discard, Review
};
enum class FindingSeverity : std::uint8_t { Information, Warning, Lossy, Fatal };

struct SourceLocator {
    std::string codecIdentifier;
    std::string sourceContainer;
    std::string sourceRecordKind;
    std::optional<std::int64_t> sourceOrdinal;
    std::optional<std::int64_t> sourceOffset;
    std::optional<std::string> rawDescriptor;
};

struct Provenance {
    std::optional<std::string> sourceFileName;
    std::optional<std::string> sourceSHA256;
    std::vector<SourceLocator> locators;
};

struct CapabilityFinding {
    FindingSeverity severity{FindingSeverity::Information};
    std::string feature;
    std::string message;
    std::optional<StableId> object;
};

struct LossReport {
    std::vector<CapabilityFinding> findings;
    [[nodiscard]] bool hasFatal() const noexcept {
        for (const auto& f : findings) if (f.severity == FindingSeverity::Fatal) return true;
        return false;
    }
    [[nodiscard]] bool isLossy() const noexcept {
        for (const auto& f : findings)
            if (f.severity == FindingSeverity::Lossy || f.severity == FindingSeverity::Fatal)
                return true;
        return false;
    }
};

} // namespace pfhorge::format
