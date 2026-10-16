#pragma once
#include <cstdint>
#include <string>

namespace psgi {
struct PackageInfo {
    std::string contentId, titleId;
    uint64_t size = 0;
    uint32_t type = 0;
};
// Read and validate the actual local PKG, independently of catalog estimates.
PackageInfo inspectPackage(const std::string& path);
std::string storagePackagePath(const std::string& path);
}
