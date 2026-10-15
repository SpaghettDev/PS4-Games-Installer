#include "storage.hpp"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdexcept>

namespace psgi {
StorageSpace mountedStorage(const std::string& path, const std::string& parent) {
    struct stat storage{}, container{};
    struct statvfs space{};
    if (stat(path.c_str(), &storage) || stat(parent.c_str(), &container)
            || !S_ISDIR(storage.st_mode) || storage.st_dev == container.st_dev
            || statvfs(path.c_str(), &space) || (space.f_flag & ST_RDONLY)) return {};
    return {true, uint64_t(space.f_bavail) * space.f_frsize};
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
InstallDestination readInstallDestination() { return InstallDestination::Unknown; }
void setInstallDestination(InstallDestination) { throw std::runtime_error("Storage selection requires a PS4"); }
#endif
}
