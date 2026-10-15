#pragma once
#include "catalog.hpp"
#include <functional>

namespace psgi {
// PS4 metadata and artwork live in separate trees. Metadata roots are also
// searched as a fallback for layouts with artwork beside the SFO.
std::vector<Entry> discoverInstalled(const std::vector<std::string>& metadataRoots,
    const std::vector<std::string>& iconRoots,
    const std::function<bool(const std::string&)>& registered,
    const std::function<uint64_t(const std::string&)>& installedSize);
}
