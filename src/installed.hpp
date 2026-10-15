#pragma once
#include "catalog.hpp"
#include <functional>

namespace psgi {
std::vector<Entry> discoverInstalled(const std::vector<std::string>& roots,
    const std::function<bool(const std::string&)>& registered,
    const std::function<uint64_t(const std::string&)>& installedSize);
}
