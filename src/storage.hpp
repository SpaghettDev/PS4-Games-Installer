#pragma once
#include <cstdint>
#include <string>

namespace psgi {
enum class InstallDestination { Unknown, Internal, External };
struct StorageSpace {
    bool available = false;
    uint64_t freeBytes = 0;
    bool sizeKnown = true;
};
struct InstallStorage {
    StorageSpace internal, external;
    InstallDestination destination = InstallDestination::Unknown;
    std::string error;
};
// External storage must be a mounted, writable PS4 Extended Storage filesystem.
// A plain USB package source is not an installation destination.
StorageSpace mountedStorage(const std::string& path, const std::string& parent);
StorageSpace filesystemSpace(const std::string& path);
InstallStorage scanInstallStorage();
InstallDestination readInstallDestination();
void setInstallDestination(InstallDestination destination);
}
