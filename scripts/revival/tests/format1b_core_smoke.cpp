// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../../Pfhorge Source/Format/Core/PfhorgeCanonicalFoundation.hpp"
#include "../../../Pfhorge Source/Format/Core/PfhorgeFormatContract.hpp"
#include <cassert>
using namespace pfhorge::format;

int main() {
    StableId id{};
    assert(id.isNil());
    LossReport report;
    assert(!report.isLossy());
    report.findings.push_back({FindingSeverity::Lossy, "editor.layers",
        "Target cannot represent Pfhorge layers.", std::nullopt});
    assert(report.isLossy());
    FormatDescriptor d;
    d.identifier = "org.pfhorge.native";
    d.filenameExtensions = {"pfhlev", "pfhscn"};
    d.capabilities.push_back({FormatOperation::SaveAs, true, false, "Native superset"});
    assert(d.capabilities.front().supported);
    return 0;
}
