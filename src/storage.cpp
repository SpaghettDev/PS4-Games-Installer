#include "storage.hpp"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdexcept>

namespace psgi {
StorageSpace mountedStorage(const std::string& path, const std::string& parent) {
    struct stat storage{}, container{};
    if (stat(path.c_str(), &storage) || stat(parent.c_str(), &container)
            || !S_ISDIR(storage.st_mode) || storage.st_dev == container.st_dev) return {};
    return filesystemSpace(path);
}
InstallStorage scanInstallStorage() {
    InstallStorage result;
    result.internal = mountedStorage("/user", "/");
    result.external = mountedStorage("/mnt/ext0", "/mnt");
    try { result.destination = readInstallDestination(); }
    catch (const std::exception& e) { result.error = e.what(); }
    return result;
}
#ifdef PSGI_HOST
StorageSpace filesystemSpace(const std::string& path) {
    struct statvfs space{};
    if (statvfs(path.c_str(), &space)) return {true, 0, false};
    if (space.f_flag & ST_RDONLY) return {};
    const uint64_t unit = space.f_frsize ? space.f_frsize : space.f_bsize;
    if (!unit || space.f_bavail > UINT64_MAX / unit) return {true, 0, false};
    return {true, uint64_t(space.f_bavail) * unit, true};
}
InstallDestination readInstallDestination() { return InstallDestination::Unknown; }
void setInstallDestination(InstallDestination) { throw std::runtime_error("Storage selection requires a PS4"); }
#endif
}
