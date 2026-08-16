// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
#pragma once
#include "PfhorgeCanonicalFoundation.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace pfhorge::format {

enum class FormatOperation : std::uint8_t {
    Probe, Open, Save, SaveAs, Import, Export, MergeSource, Validate, RoundTrip
};

struct FormatCapability {
    FormatOperation operation{FormatOperation::Probe};
    bool supported{false};
    bool potentiallyLossy{false};
    std::string note;
};

struct FormatDescriptor {
    std::string identifier;
    std::string displayName;
    std::vector<std::string> filenameExtensions;
    std::vector<std::string> mediaTypes;
    std::vector<FormatCapability> capabilities;
};

struct ValidationResult {
    bool structurallyValid{false};
    bool semanticallyValid{false};
    LossReport report;
};

class FormatCodecContract {
public:
    virtual ~FormatCodecContract() = default;
    [[nodiscard]] virtual const FormatDescriptor& descriptor() const noexcept = 0;
};

} // namespace pfhorge::format
