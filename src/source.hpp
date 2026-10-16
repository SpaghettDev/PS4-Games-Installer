#pragma once
#include "catalog.hpp"

namespace psgi {
struct SourceReport {
    bool success = false;
    Source source;
    size_t packages = 0, games = 0, updates = 0, dlc = 0, missingIds = 0;
    std::string message;
};
SourceReport inspectSource(const Source& source, const std::string& document);
std::string sourceFailure(const Source& source, const std::string& error);
}
